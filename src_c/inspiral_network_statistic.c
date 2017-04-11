/*
 * coherent_new_sec2.c
 *
 *  Created on: Feb 23, 2017
 *      Author: marcnormandin
 */

#include <math.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <gsl/gsl_math.h>
#include <gsl/gsl_cblas.h>
#include <gsl/gsl_complex.h>
#include <gsl/gsl_complex_math.h>
#include <gsl/gsl_fft_complex.h>
#include <gsl/gsl_statistics_double.h>

#include "strain.h"
#include "strain_interpolate.h"
#include "inspiral_network_statistic.h"
#include "detector.h"
#include "detector_antenna_patterns.h"
#include "detector_network.h"
#include "detector_time_delay.h"
#include "sampling_system.h"

coherent_network_helper_t* CN_helper_malloc(size_t num_time_samples) {
	coherent_network_helper_t *h;

	h = (coherent_network_helper_t*) malloc( sizeof(coherent_network_helper_t) );

	/*fprintf(stderr, "c_plus and c_plus have a length of: %d\n", s);*/

	h->c_plus = (gsl_complex*) malloc( num_time_samples * sizeof(gsl_complex) );
	h->c_minus = (gsl_complex*) malloc( num_time_samples * sizeof(gsl_complex) );
	return h;
}

void CN_helper_free( coherent_network_helper_t* helper) {
	free(helper->c_plus);
	free(helper->c_minus);
	free(helper);
}

/* Note, the asd is only needed to get the frequency values and the number of frequency bins. This should be the
 * same for every ASD used for a detector network, so any detector from the network can be used.
 */
coherent_network_workspace_t* CN_workspace_malloc(size_t num_time_samples, size_t num_detectors, size_t num_half_freq,
		double f_low, double f_high, asd_t *asd) {
	coherent_network_workspace_t * work;
	size_t i;

	work = (coherent_network_workspace_t*) malloc(sizeof(coherent_network_workspace_t));
	work->num_time_samples = num_time_samples;
	work->num_helpers = num_detectors;
	work->helpers = (coherent_network_helper_t**) malloc( work->num_helpers * sizeof(coherent_network_helper_t*));
	for (i = 0; i < work->num_helpers; i++) {
		work->helpers[i] = CN_helper_malloc( num_time_samples );
	}

	work->sp_lookup = SP_lookup_alloc(f_low, f_high, asd);
	SP_lookup_init(f_low, f_high, asd, work->sp_lookup);

	work->sp = SP_malloc( num_half_freq );

	work->temp_array = (gsl_complex*) malloc( num_half_freq * sizeof(gsl_complex) );

	work->terms = (gsl_complex**) malloc(4 * sizeof(gsl_complex*) );

	/*fprintf(stderr, "len_terms has a length of: %d\n", len_terms);*/

	for (i = 0; i < 4; i++) {
		work->terms[i] = (gsl_complex*) malloc( num_time_samples * sizeof(gsl_complex) );
	}

	work->fs = (double**) malloc( 4 * sizeof(double*) );
	for (i = 0; i < 4; i++) {
		work->fs[i] = (double*) malloc( 2 * num_time_samples * sizeof(double) );
	}

	work->temp_ifft = (double*) malloc( num_time_samples * sizeof(double) );

	/*fprintf(stderr, "temp_ifft has a length of: %d\n", s);*/

	work->fft_wavetable = gsl_fft_complex_wavetable_alloc( num_time_samples );
	work->fft_workspace = gsl_fft_complex_workspace_alloc( num_time_samples );

	work->ap_workspace = antenna_patterns_workspace_alloc();
	/* one antenna pattern structure per detector */
	work->ap = (antenna_patterns_t*) malloc (num_detectors * sizeof(antenna_patterns_t) );

	return work;
}

void CN_workspace_free( coherent_network_workspace_t *workspace ) {
	size_t i;

	for (i = 0; i < workspace->num_helpers; i++) {
		CN_helper_free( workspace->helpers[i] );
	}
	free(workspace->helpers);

	SP_lookup_free(workspace->sp_lookup);
	SP_free(workspace->sp);

	for (i = 0; i < 4; i++) {
		free(workspace->terms[i]);
		free(workspace->fs[i]);
	}

	free(workspace->terms);
	free(workspace->fs);
	free(workspace->temp_ifft);
	free(workspace->temp_array);
	gsl_fft_complex_workspace_free( workspace->fft_workspace );
	gsl_fft_complex_wavetable_free( workspace->fft_wavetable );

	antenna_patterns_workspace_free(workspace->ap_workspace);
	free(workspace->ap);

	free( workspace );
}

void do_work(size_t num_time_samples, gsl_complex *spa, asd_t *asd, gsl_complex *half_fft_data, gsl_complex *temp, gsl_complex *out_c) {
	size_t k;
	size_t t_index;
	size_t c_index;

	for (k = 0; k < asd->len; k++) {
		temp[k] = gsl_complex_conjugate(spa[k]);
		temp[k] = gsl_complex_div_real(temp[k], asd->asd[k]);
		temp[k] = gsl_complex_mul( temp[k], half_fft_data[k] );
	}

	/* This should extend the array with a flipped conjugated version that has 2 less elements. */
	/*
	t_index = regular_strain->len - 2;
	c_index = regular_strain->len;
	for (; t_index > 0; t_index--, c_index++) {
		out_c[c_index] = gsl_complex_conjugate(temp[t_index]);
	}
	*/

	/*fprintf(stderr, "out_c, N has a length of: %d\n", N);*/
	/*fprintf(stderr, "do_work: num_time_samples = %lu\n", num_time_samples);*/

	SS_make_two_sided( asd->len, temp, num_time_samples, out_c);
}

void CN_save(char* filename, size_t len, double* tmp_ifft) {
	FILE* file;
	size_t i;

	file = fopen(filename, "w");
	for (i = 0; i < len; i++) {
		fprintf(file, "%e\n", tmp_ifft[i]);
	}
	fclose(file);
}

void coherent_network_statistic(
		detector_network_t* net,
		double f_low,
		double f_high,
		chirp_time_t *chirp,
		sky_t *sky,
		double polarization_angle,
		inspiral_template_half_fft_t **signals,
		coherent_network_workspace_t *workspace,
		double *out_val)
{
	double UdotU_input;
	double UdotV_input;
	double VdotV_input;
	size_t i;
	double A_input;
	double B_input;
	double C_input;
	double Delta_input;
	double Delta_factor_input;
	double D_input;
	double P1_input;
	double P2_input;
	double P3_input;
	double P4_input;
	double G1_input;
	double G2_input;
	double O11_input;
	double O12_input;
	double O21_input;
	double O22_input;
	size_t tid;
	size_t did;
	size_t fid;
	size_t j;
	double max;

	/* WARNING: This assumes that all of the signals have the same lengths. */
	size_t num_time_samples = signals[0]->full_len;
	/*fprintf(stderr, "inspiral network statistic: num_time_samples = %lu\n", num_time_samples);*/

	/* Compute the antenna patterns for each detector */
	for (i = 0; i < net->num_detectors; i++) {
		antenna_patterns(net->detector[i], sky, polarization_angle,
				workspace->ap_workspace, &workspace->ap[i]);
	}

	/* We need to make vectors with the same number of dimensions as the number of detectors in the network */
	UdotU_input = 0.0;
	UdotV_input = 0.0;
	VdotV_input = 0.0;

	/* dot product */
	for (i = 0; i < net->num_detectors; i++) {
		UdotU_input += workspace->ap[i].u * workspace->ap[i].u;
		UdotV_input += workspace->ap[i].u * workspace->ap[i].v;
		VdotV_input += workspace->ap[i].v * workspace->ap[i].v;
	}

	A_input = UdotU_input;
	B_input = UdotV_input;
	C_input = VdotV_input;

	Delta_input = (A_input*C_input) - (B_input*B_input);
	Delta_factor_input = 1.0 / sqrt(2.0*Delta_input);

	D_input = sqrt( gsl_pow_2(A_input-C_input) + 4.0*gsl_pow_2(B_input) ) ;

	P1_input = (C_input-A_input-D_input);
	P2_input = (C_input-A_input+D_input);
	P3_input = sqrt(C_input+A_input+D_input);
	P4_input = sqrt(C_input+A_input-D_input);

	G1_input =  sqrt( gsl_pow_2(P1_input) +  4.0*gsl_pow_2(B_input)) / (2.0*B_input) ;
	G2_input =  sqrt( gsl_pow_2(P2_input) +  4.0*gsl_pow_2(B_input)) / (2.0*B_input) ;

	O11_input = Delta_factor_input * P3_input / G1_input ;
	O12_input = Delta_factor_input * P3_input * P1_input / (2.0*B_input*G1_input) ;
	O21_input = Delta_factor_input * P4_input / G2_input ;
	O22_input  = Delta_factor_input * P4_input * P2_input / (2.0*B_input*G2_input) ;

	/* Loop over each detector to generate a template and do matched filtering */
	for (i = 0; i < net->num_detectors; i++) {
		detector_t* det;
		double coalesce_phase;
		gsl_complex* whitened_data;
		double U_vec_input;
		double V_vec_input;
		double td;

		det = net->detector[i];

		coalesce_phase = 0.0;

		/* Compute time delay */
		time_delay(det, sky, &td);

		SP_compute(coalesce_phase, td,
						chirp, det->asd,
						f_low, f_high,
						workspace->sp_lookup,
						workspace->sp);

		whitened_data = signals[i]->half_fft;

		do_work(num_time_samples, workspace->sp->spa_0, det->asd, whitened_data, workspace->temp_array, workspace->helpers[i]->c_plus);

		do_work(num_time_samples, workspace->sp->spa_90, det->asd, whitened_data, workspace->temp_array, workspace->helpers[i]->c_minus);

		U_vec_input = workspace->ap[i].u;
		V_vec_input = workspace->ap[i].v;

		workspace->helpers[i]->w_plus_input = (O11_input*U_vec_input +  O12_input*V_vec_input);
		workspace->helpers[i]->w_minus_input = (O21_input*U_vec_input +  O22_input*V_vec_input);
	}

	/* zero the memory */
	for (tid = 0; tid < 4; tid++) {
		memset( workspace->terms[tid], 0, num_time_samples * sizeof(gsl_complex) );
		memset( workspace->fs[tid], 0, num_time_samples * sizeof(gsl_complex) );
	}

	for (did = 0; did < net->num_detectors; did++) {
		for (fid = 0; fid < num_time_samples; fid++) {
			gsl_complex t;

			t = gsl_complex_mul_real(workspace->helpers[did]->c_plus[fid], workspace->helpers[did]->w_plus_input);
			workspace->terms[0][fid] = gsl_complex_add( workspace->terms[0][fid], t);

			t = gsl_complex_mul_real(workspace->helpers[did]->c_plus[fid], workspace->helpers[did]->w_minus_input);
			workspace->terms[1][fid] = gsl_complex_add( workspace->terms[1][fid], t);

			t = gsl_complex_mul_real(workspace->helpers[did]->c_minus[fid], workspace->helpers[did]->w_plus_input);
			workspace->terms[2][fid] = gsl_complex_add( workspace->terms[2][fid], t);

			t = gsl_complex_mul_real(workspace->helpers[did]->c_minus[fid], workspace->helpers[did]->w_minus_input);
			workspace->terms[3][fid] = gsl_complex_add( workspace->terms[3][fid], t);
		}
	}

	for (i = 0; i < 4; i++) {
		for (j = 0; j < num_time_samples; j++) {
			workspace->fs[i][2*j + 0] = GSL_REAL( workspace->terms[i][j] );
			workspace->fs[i][2*j + 1] = GSL_IMAG( workspace->terms[i][j] );
		}
		gsl_fft_complex_inverse( workspace->fs[i], 1, num_time_samples, workspace->fft_wavetable, workspace->fft_workspace );
	}

	memset(workspace->temp_ifft, 0, num_time_samples * sizeof(double));
	for (i = 0; i < 4; i++) {
		for (j = 0; j < num_time_samples; j++) {
			/* Take only the real part. The imaginary part should be zero. */
			double x = workspace->fs[i][2*j + 0];
			workspace->temp_ifft[j] += gsl_pow_2(x*num_time_samples);
		}
	}

	/*CN_save("tmp_ifft.dat", s, workspace->temp_ifft);*/

	max = workspace->temp_ifft[0];
	/* check statistical behavior of this time series */
	for (i = 1; i < num_time_samples; i++) {
		double m = workspace->temp_ifft[i];
		if (m > max) {
			max = m;
		}
	}

	/* divide the standard deviation to convert to SNR */
	/*double std = gsl_stats_sd(workspace->temp_ifft, 1, s);

	*out_val = sqrt(max) / std;*/
	*out_val = sqrt(max);
	/* check, sqrt sbould behave accordding to chi */
	/* check, use this with just noise and see if the mean is 4, std should be sqrt(8). Chi-sqre if not sqrt(max). Check 'max' dist.*/

	/* *out_val = max; */
	/* *out_val = max / std; */
}