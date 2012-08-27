/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2012 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin,
 *                 Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/mask_evaluate.c
 *  \ingroup bke
 *
 * Functions for evaluating the mask beziers into points for the outline and feather.
 */

#include <stddef.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_listbase.h"
#include "BLI_math.h"

#include "DNA_mask_types.h"
#include "DNA_node_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_tracking_types.h"
#include "DNA_sequence_types.h"

#include "BKE_curve.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_mask.h"
#include "BKE_node.h"
#include "BKE_sequencer.h"
#include "BKE_tracking.h"
#include "BKE_movieclip.h"
#include "BKE_utildefines.h"


unsigned int BKE_mask_spline_resolution(MaskSpline *spline, int width, int height)
{
	float max_segment = 0.01f;
	unsigned int i, resol = 1;

	if (width != 0 && height != 0) {
		max_segment = 1.0f / (float)maxi(width, height);
	}

	for (i = 0; i < spline->tot_point; i++) {
		MaskSplinePoint *point = &spline->points[i];
		BezTriple *bezt_curr, *bezt_next;
		float a, b, c, len;
		unsigned int cur_resol;

		bezt_curr = &point->bezt;
		bezt_next = BKE_mask_spline_point_next_bezt(spline, spline->points, point);

		if (bezt_next == NULL) {
			break;
		}

		a = len_v3v3(bezt_curr->vec[1], bezt_curr->vec[2]);
		b = len_v3v3(bezt_curr->vec[2], bezt_next->vec[0]);
		c = len_v3v3(bezt_next->vec[0], bezt_next->vec[1]);

		len = a + b + c;
		cur_resol = len / max_segment;

		resol = MAX2(resol, cur_resol);

		if (resol >= MASK_RESOL_MAX) {
			break;
		}
	}

	return CLAMPIS(resol, 1, MASK_RESOL_MAX);
}

unsigned int BKE_mask_spline_feather_resolution(MaskSpline *spline, int width, int height)
{
	const float max_segment = 0.005;
	unsigned int resol = BKE_mask_spline_resolution(spline, width, height);
	float max_jump = 0.0f;
	int i;

	/* avoid checking the featrher if we already hit the maximum value */
	if (resol >= MASK_RESOL_MAX) {
		return MASK_RESOL_MAX;
	}

	for (i = 0; i < spline->tot_point; i++) {
		MaskSplinePoint *point = &spline->points[i];
		float prev_u, prev_w;
		int j;

		prev_u = 0.0f;
		prev_w = point->bezt.weight;

		for (j = 0; j < point->tot_uw; j++) {
			const float w_diff = (point->uw[j].w - prev_w);
			const float u_diff = (point->uw[j].u - prev_u);

			/* avoid divide by zero and very high values,
			 * though these get clamped eventually */
			if (u_diff > FLT_EPSILON) {
				float jump = fabsf(w_diff / u_diff);

				max_jump = MAX2(max_jump, jump);
			}

			prev_u = point->uw[j].u;
			prev_w = point->uw[j].w;
		}
	}

	resol += max_jump / max_segment;

	return CLAMPIS(resol, 1, MASK_RESOL_MAX);
}

int BKE_mask_spline_differentiate_calc_total(const MaskSpline *spline, const unsigned int resol)
{
	if (spline->flag & MASK_SPLINE_CYCLIC) {
		return spline->tot_point * resol;
	}
	else {
		return ((spline->tot_point - 1) * resol) + 1;
	}
}

float (*BKE_mask_spline_differentiate_with_resolution_ex(MaskSpline *spline,
                                                         int *tot_diff_point,
                                                         const unsigned int resol
                                                         ))[2]
{
	MaskSplinePoint *points_array = BKE_mask_spline_point_array(spline);

	MaskSplinePoint *point_curr, *point_prev;
	float (*diff_points)[2], (*fp)[2];
	const int tot = BKE_mask_spline_differentiate_calc_total(spline, resol);
	int a;

	if (spline->tot_point <= 1) {
		/* nothing to differentiate */
		*tot_diff_point = 0;
		return NULL;
	}

	/* len+1 because of 'forward_diff_bezier' function */
	*tot_diff_point = tot;
	diff_points = fp = MEM_mallocN((tot + 1) * sizeof(*diff_points), "mask spline vets");

	a = spline->tot_point - 1;
	if (spline->flag & MASK_SPLINE_CYCLIC)
		a++;

	point_prev = points_array;
	point_curr = point_prev + 1;

	while (a--) {
		BezTriple *bezt_prev;
		BezTriple *bezt_curr;
		int j;

		if (a == 0 && (spline->flag & MASK_SPLINE_CYCLIC))
			point_curr = points_array;

		bezt_prev = &point_prev->bezt;
		bezt_curr = &point_curr->bezt;

		for (j = 0; j < 2; j++) {
			BKE_curve_forward_diff_bezier(bezt_prev->vec[1][j], bezt_prev->vec[2][j],
			                              bezt_curr->vec[0][j], bezt_curr->vec[1][j],
			                              &(*fp)[j], resol, 2 * sizeof(float));
		}

		fp += resol;

		if (a == 0 && (spline->flag & MASK_SPLINE_CYCLIC) == 0) {
			copy_v2_v2(*fp, bezt_curr->vec[1]);
		}

		point_prev = point_curr;
		point_curr++;
	}

	return diff_points;
}

float (*BKE_mask_spline_differentiate_with_resolution(MaskSpline *spline, int width, int height,
                                                      int *tot_diff_point
                                                      ))[2]
{
	int unsigned resol = BKE_mask_spline_resolution(spline, width, height);

	return BKE_mask_spline_differentiate_with_resolution_ex(spline, tot_diff_point, resol);
}

float (*BKE_mask_spline_differentiate(MaskSpline *spline, int *tot_diff_point))[2]
{
	return BKE_mask_spline_differentiate_with_resolution(spline, 0, 0, tot_diff_point);
}

/* ** feather points self-intersection collapse routine ** */

typedef struct FeatherEdgesBucket {
	int tot_segment;
	int (*segments)[2];
	int alloc_segment;
} FeatherEdgesBucket;

static void feather_bucket_add_edge(FeatherEdgesBucket *bucket, int start, int end)
{
	const int alloc_delta = 256;

	if (bucket->tot_segment >= bucket->alloc_segment) {
		if (!bucket->segments) {
			bucket->segments = MEM_callocN(alloc_delta * sizeof(*bucket->segments), "feather bucket segments");
		}
		else {
			bucket->segments = MEM_reallocN(bucket->segments,
					(alloc_delta + bucket->tot_segment) * sizeof(*bucket->segments));
		}

		bucket->alloc_segment += alloc_delta;
	}

	bucket->segments[bucket->tot_segment][0] = start;
	bucket->segments[bucket->tot_segment][1] = end;

	bucket->tot_segment++;
}

static void feather_bucket_check_intersect(float (*feather_points)[2], int tot_feather_point, FeatherEdgesBucket *bucket,
                                           int cur_a, int cur_b)
{
	int i;

	float *v1 = (float *) feather_points[cur_a];
	float *v2 = (float *) feather_points[cur_b];

	for (i = 0; i < bucket->tot_segment; i++) {
		int check_a = bucket->segments[i][0];
		int check_b = bucket->segments[i][1];

		float *v3 = (float *) feather_points[check_a];
		float *v4 = (float *) feather_points[check_b];

		if (check_a >= cur_a - 1 || cur_b == check_a)
			continue;

		if (isect_seg_seg_v2(v1, v2, v3, v4)) {
			int k;
			float p[2];
			float min_a[2], max_a[2];
			float min_b[2], max_b[2];

			isect_seg_seg_v2_point(v1, v2, v3, v4, p);

			INIT_MINMAX2(min_a, max_a);
			INIT_MINMAX2(min_b, max_b);

			/* collapse loop with smaller AABB */
			for (k = 0; k < tot_feather_point; k++) {
				if (k >= check_b && k <= cur_a) {
					DO_MINMAX2(feather_points[k], min_a, max_a);
				}
				else {
					DO_MINMAX2(feather_points[k], min_b, max_b);
				}
			}

			if (max_a[0] - min_a[0] < max_b[0] - min_b[0] ||
			    max_a[1] - min_a[1] < max_b[1] - min_b[1])
			{
				for (k = check_b; k <= cur_a; k++) {
					copy_v2_v2(feather_points[k], p);
				}
			}
			else {
				for (k = 0; k <= check_a; k++) {
					copy_v2_v2(feather_points[k], p);
				}

				if (cur_b != 0) {
					for (k = cur_b; k < tot_feather_point; k++) {
						copy_v2_v2(feather_points[k], p);
					}
				}
			}
		}
	}
}

static int feather_bucket_index_from_coord(float co[2], const float min[2], const float bucket_scale[2],
                                           const int buckets_per_side)
{
	int x = (int) ((co[0] - min[0]) * bucket_scale[0]);
	int y = (int) ((co[1] - min[1]) * bucket_scale[1]);

	if (x == buckets_per_side)
		x--;

	if (y == buckets_per_side)
		y--;

	return y * buckets_per_side + x;
}

static void feather_bucket_get_diagonal(FeatherEdgesBucket *buckets, int start_bucket_index, int end_bucket_index,
                                        int buckets_per_side, FeatherEdgesBucket **diagonal_bucket_a_r,
                                        FeatherEdgesBucket **diagonal_bucket_b_r)
{
	int start_bucket_x = start_bucket_index % buckets_per_side;
	int start_bucket_y = start_bucket_index / buckets_per_side;

	int end_bucket_x = end_bucket_index % buckets_per_side;
	int end_bucket_y = end_bucket_index / buckets_per_side;

	int diagonal_bucket_a_index = start_bucket_y * buckets_per_side + end_bucket_x;
	int diagonal_bucket_b_index = end_bucket_y * buckets_per_side + start_bucket_x;

	*diagonal_bucket_a_r = &buckets[diagonal_bucket_a_index];
	*diagonal_bucket_b_r = &buckets[diagonal_bucket_b_index];
}

void BKE_mask_spline_feather_collapse_inner_loops(MaskSpline *spline, float (*feather_points)[2], const int tot_feather_point)
{
#define BUCKET_INDEX(co) \
	feather_bucket_index_from_coord(co, min, bucket_scale, buckets_per_side)

	int buckets_per_side, tot_bucket;
	float bucket_size, bucket_scale[2];

	FeatherEdgesBucket *buckets;

	int i;
	float min[2], max[2];
	float max_delta_x = -1.0f, max_delta_y = -1.0f, max_delta;

	if (tot_feather_point < 4) {
		/* self-intersection works only for quads at least,
		 * in other cases polygon can't be self-intersecting anyway
		 */

		return;
	}

	/* find min/max corners of mask to build buckets in that space */
	INIT_MINMAX2(min, max);

	for (i = 0; i < tot_feather_point; i++) {
		int next = i + 1;
		float delta;

		DO_MINMAX2(feather_points[i], min, max);

		if (next == tot_feather_point) {
			if (spline->flag & MASK_SPLINE_CYCLIC)
				next = 0;
			else
				break;
		}

		delta = fabsf(feather_points[i][0] - feather_points[next][0]);
		if (delta > max_delta_x)
			max_delta_x = delta;

		delta = fabsf(feather_points[i][1] - feather_points[next][1]);
		if (delta > max_delta_y)
			max_delta_y = delta;
	}

	/* prevent divisionsby zero by ensuring bounding box is not collapsed */
	if (max[0] - min[0] < FLT_EPSILON) {
		max[0] += 0.01f;
		min[0] -= 0.01f;
	}

	if (max[1] - min[1] < FLT_EPSILON) {
		max[1] += 0.01f;
		min[1] -= 0.01f;
	}

	/* use dynamically calculated buckets per side, so we likely wouldn't
	 * run into a situation when segment doesn't fit two buckets which is
	 * pain collecting candidates for intersection
	 */

	max_delta_x /= max[0] - min[0];
	max_delta_y /= max[1] - min[1];

	max_delta = MAX2(max_delta_x, max_delta_y);

	buckets_per_side = MIN2(512, 0.9f / max_delta);

	if (buckets_per_side == 0) {
		/* happens when some segment fills the whole bounding box across some of dimension */

		buckets_per_side = 1;
	}

	tot_bucket = buckets_per_side * buckets_per_side;
	bucket_size = 1.0f / buckets_per_side;

	/* pre-compute multipliers, to save mathematical operations in loops */
	bucket_scale[0] = 1.0f / ((max[0] - min[0]) * bucket_size);
	bucket_scale[1] = 1.0f / ((max[1] - min[1]) * bucket_size);

	/* fill in buckets' edges */
	buckets = MEM_callocN(sizeof(FeatherEdgesBucket) * tot_bucket, "feather buckets");

	for (i = 0; i < tot_feather_point; i++) {
		int start = i, end = i + 1;
		int start_bucket_index, end_bucket_index;

		if (end == tot_feather_point) {
			if (spline->flag & MASK_SPLINE_CYCLIC)
				end = 0;
			else
				break;
		}

		start_bucket_index = BUCKET_INDEX(feather_points[start]);
		end_bucket_index = BUCKET_INDEX(feather_points[end]);

		feather_bucket_add_edge(&buckets[start_bucket_index], start, end);

		if (start_bucket_index != end_bucket_index) {
			FeatherEdgesBucket *end_bucket = &buckets[end_bucket_index];
			FeatherEdgesBucket *diagonal_bucket_a, *diagonal_bucket_b;

			feather_bucket_get_diagonal(buckets, start_bucket_index, end_bucket_index, buckets_per_side,
			                            &diagonal_bucket_a, &diagonal_bucket_b);

			feather_bucket_add_edge(end_bucket, start, end);
			feather_bucket_add_edge(diagonal_bucket_a, start, end);
			feather_bucket_add_edge(diagonal_bucket_a, start, end);
		}
	}

	/* check all edges for intersection with edges from their buckets */
	for (i = 0; i < tot_feather_point; i++) {
		int cur_a = i, cur_b = i + 1;
		int start_bucket_index, end_bucket_index;

		FeatherEdgesBucket *start_bucket;

		if (cur_b == tot_feather_point)
			cur_b = 0;

		start_bucket_index = BUCKET_INDEX(feather_points[cur_a]);
		end_bucket_index = BUCKET_INDEX(feather_points[cur_b]);

		start_bucket = &buckets[start_bucket_index];

		feather_bucket_check_intersect(feather_points, tot_feather_point, start_bucket, cur_a, cur_b);

		if (start_bucket_index != end_bucket_index) {
			FeatherEdgesBucket *end_bucket = &buckets[end_bucket_index];
			FeatherEdgesBucket *diagonal_bucket_a, *diagonal_bucket_b;

			feather_bucket_get_diagonal(buckets, start_bucket_index, end_bucket_index, buckets_per_side,
			                            &diagonal_bucket_a, &diagonal_bucket_b);

			feather_bucket_check_intersect(feather_points, tot_feather_point, end_bucket, cur_a, cur_b);
			feather_bucket_check_intersect(feather_points, tot_feather_point, diagonal_bucket_a, cur_a, cur_b);
			feather_bucket_check_intersect(feather_points, tot_feather_point, diagonal_bucket_b, cur_a, cur_b);
		}
	}

	/* free buckets */
	for (i = 0; i < tot_bucket; i++) {
		if (buckets[i].segments)
			MEM_freeN(buckets[i].segments);
	}

	MEM_freeN(buckets);

#undef BUCKET_INDEX
}

/**
 * values align with #BKE_mask_spline_differentiate_with_resolution_ex
 * when \a resol arguments match.
 */
float (*BKE_mask_spline_feather_differentiated_points_with_resolution_ex(MaskSpline *spline,
                                                                         int *tot_feather_point,
                                                                         const unsigned int resol,
                                                                         const int do_feather_isect
                                                                         ))[2]
{
	MaskSplinePoint *points_array = BKE_mask_spline_point_array(spline);
	MaskSplinePoint *point_curr, *point_prev;
	float (*feather)[2], (*fp)[2];

	const int tot = BKE_mask_spline_differentiate_calc_total(spline, resol);
	int a;

	/* tot+1 because of 'forward_diff_bezier' function */
	feather = fp = MEM_mallocN((tot + 1) * sizeof(*feather), "mask spline feather diff points");

	a = spline->tot_point - 1;
	if (spline->flag & MASK_SPLINE_CYCLIC)
		a++;

	point_prev = points_array;
	point_curr = point_prev + 1;

	while (a--) {
		/* BezTriple *bezt_prev; */  /* UNUSED */
		/* BezTriple *bezt_curr; */      /* UNUSED */
		int j;

		if (a == 0 && (spline->flag & MASK_SPLINE_CYCLIC))
			point_curr = points_array;


		/* bezt_prev = &point_prev->bezt; */
		/* bezt_curr = &point_curr->bezt; */

		for (j = 0; j < resol; j++, fp++) {
			float u = (float) j / resol, weight;
			float co[2], n[2];

			/* TODO - these calls all calculate similar things
			 * could be unified for some speed */
			BKE_mask_point_segment_co(spline, point_prev, u, co);
			BKE_mask_point_normal(spline, point_prev, u, n);
			weight = BKE_mask_point_weight(spline, point_prev, u);

			madd_v2_v2v2fl(*fp, co, n, weight);
		}

		if (a == 0 && (spline->flag & MASK_SPLINE_CYCLIC) == 0) {
			float u = 1.0f, weight;
			float co[2], n[2];

			BKE_mask_point_segment_co(spline, point_prev, u, co);
			BKE_mask_point_normal(spline, point_prev, u, n);
			weight = BKE_mask_point_weight(spline, point_prev, u);

			madd_v2_v2v2fl(*fp, co, n, weight);
		}

		point_prev = point_curr;
		point_curr++;
	}

	*tot_feather_point = tot;

	if ((spline->flag & MASK_SPLINE_NOINTERSECT) && do_feather_isect) {
		BKE_mask_spline_feather_collapse_inner_loops(spline, feather, tot);
	}

	return feather;
}

float (*BKE_mask_spline_feather_differentiated_points_with_resolution(MaskSpline *spline, int width, int height,
                                                                      int *tot_feather_point, const int do_feather_isect))[2]
{
	unsigned int resol = BKE_mask_spline_feather_resolution(spline, width, height);

	return BKE_mask_spline_feather_differentiated_points_with_resolution_ex(spline, tot_feather_point, resol, do_feather_isect);
}

float (*BKE_mask_spline_feather_differentiated_points(MaskSpline *spline, int *tot_feather_point))[2]
{
	return BKE_mask_spline_feather_differentiated_points_with_resolution(spline, 0, 0, tot_feather_point, TRUE);
}

float (*BKE_mask_spline_feather_points(MaskSpline *spline, int *tot_feather_point))[2]
{
	MaskSplinePoint *points_array = BKE_mask_spline_point_array(spline);

	int i, tot = 0;
	float (*feather)[2], (*fp)[2];

	/* count */
	for (i = 0; i < spline->tot_point; i++) {
		MaskSplinePoint *point = &points_array[i];

		tot += point->tot_uw + 1;
	}

	/* create data */
	feather = fp = MEM_mallocN(tot * sizeof(*feather), "mask spline feather points");

	for (i = 0; i < spline->tot_point; i++) {
		MaskSplinePoint *point = &points_array[i];
		BezTriple *bezt = &point->bezt;
		float weight, n[2];
		int j;

		BKE_mask_point_normal(spline, point, 0.0f, n);
		weight = BKE_mask_point_weight(spline, point, 0.0f);

		madd_v2_v2v2fl(*fp, bezt->vec[1], n, weight);
		fp++;

		for (j = 0; j < point->tot_uw; j++) {
			float u = point->uw[j].u;
			float co[2];

			BKE_mask_point_segment_co(spline, point, u, co);
			BKE_mask_point_normal(spline, point, u, n);
			weight = BKE_mask_point_weight(spline, point, u);

			madd_v2_v2v2fl(*fp, co, n, weight);
			fp++;
		}
	}

	*tot_feather_point = tot;

	return feather;
}

/* *** mask point functions which involve evaluation *** */
float *BKE_mask_point_segment_feather_diff_with_resolution(MaskSpline *spline, MaskSplinePoint *point,
                                                           int width, int height,
                                                           unsigned int *tot_feather_point)
{
	float *feather, *fp;
	unsigned int resol = BKE_mask_spline_feather_resolution(spline, width, height);
	unsigned int i;

	feather = fp = MEM_callocN(2 * resol * sizeof(float), "mask point spline feather diff points");

	for (i = 0; i < resol; i++, fp += 2) {
		float u = (float)(i % resol) / resol, weight;
		float co[2], n[2];

		BKE_mask_point_segment_co(spline, point, u, co);
		BKE_mask_point_normal(spline, point, u, n);
		weight = BKE_mask_point_weight(spline, point, u);

		fp[0] = co[0] + n[0] * weight;
		fp[1] = co[1] + n[1] * weight;
	}

	*tot_feather_point = resol;

	return feather;
}

float *BKE_mask_point_segment_feather_diff(MaskSpline *spline, MaskSplinePoint *point, unsigned int *tot_feather_point)
{
	return BKE_mask_point_segment_feather_diff_with_resolution(spline, point, 0, 0, tot_feather_point);
}

float *BKE_mask_point_segment_diff_with_resolution(MaskSpline *spline, MaskSplinePoint *point,
                                                   int width, int height, unsigned int *tot_diff_point)
{
	MaskSplinePoint *points_array = BKE_mask_spline_point_array_from_point(spline, point);

	BezTriple *bezt, *bezt_next;
	float *diff_points, *fp;
	int j, resol = BKE_mask_spline_resolution(spline, width, height);

	bezt = &point->bezt;
	bezt_next = BKE_mask_spline_point_next_bezt(spline, points_array, point);

	if (!bezt_next)
		return NULL;

	/* resol+1 because of 'forward_diff_bezier' function */
	*tot_diff_point = resol + 1;
	diff_points = fp = MEM_callocN((resol + 1) * 2 * sizeof(float), "mask segment vets");

	for (j = 0; j < 2; j++) {
		BKE_curve_forward_diff_bezier(bezt->vec[1][j], bezt->vec[2][j],
		                              bezt_next->vec[0][j], bezt_next->vec[1][j],
		                              fp + j, resol, 2 * sizeof(float));
	}

	copy_v2_v2(fp + 2 * resol, bezt_next->vec[1]);

	return diff_points;
}

float *BKE_mask_point_segment_diff(MaskSpline *spline, MaskSplinePoint *point, unsigned int *tot_diff_point)
{
	return BKE_mask_point_segment_diff_with_resolution(spline, point, 0, 0, tot_diff_point);
}