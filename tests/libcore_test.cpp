/*
 * libcore_test.c
 *
 *  Created on: Apr 15, 2017
 *      Author: marcnormandin
 */

#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif

#ifdef HAVE_GTEST
	#include <gtest/gtest.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gsl/gsl_complex.h>
#include <gsl/gsl_complex_math.h>
#include <gsl/gsl_const_mksa.h>

#include "../libcore/sky.h"
#include "../libcore/detector_antenna_patterns.h"
#include "../libcore/detector_mapping.h"
#include "../libcore/detector_network.h"
#include "../libcore/detector_time_delay.h"
#include "../libcore/detector.h"
#include "../libcore/hdf5_file.h"
#include "../libcore/inspiral_chirp_factors.h"
#include "../libcore/inspiral_chirp.h"
#include "../libcore/inspiral_network_statistic.h"
#include "../libcore/inspiral_stationary_phase.h"
#include "../libcore/random.h"
#include "../libcore/sampling_system.h"
#include "../libcore/settings_file.h"
#include "../libcore/spectral_density.h"
#include "../libcore/strain.h"

#ifdef HAVE_GTEST

TEST(SS_make_two_sided, matchesMatlab) {
	size_t M = 6;
	size_t N = 10;
	gsl_complex *one_sided = (gsl_complex*) malloc( M * sizeof(gsl_complex) );
	for (int i = 0; i < M; i++) {
		one_sided[i] = gsl_complex_rect(i, i);
	}

	gsl_complex *two_sided = (gsl_complex*) malloc( N * sizeof(gsl_complex) );

	SS_make_two_sided (M, one_sided, N, two_sided);

	for (int i = 0; i < M; i++) {
		EXPECT_EQ(GSL_REAL(two_sided[i]), i);
		EXPECT_EQ(GSL_IMAG(two_sided[i]), i);
	}

	// the conjugated half
	for (int i = M; i < N; i++) {
		EXPECT_EQ(GSL_REAL(two_sided[i]), GSL_REAL(two_sided[N-i]));
		EXPECT_EQ(GSL_IMAG(two_sided[i]), -1.0*GSL_IMAG(two_sided[N-i]));
	}
}

TEST(find_index_low, left_end) {
	size_t N = 100;
	double f_array[N];
	for (int i = 0; i < N; i++) {
		f_array[i] = i;
	}

	double f_low = 0.0;
	int index = find_index_low(f_low, N, f_array);

	EXPECT_EQ(index, 0);
}

TEST(find_index_low, right_end) {
	size_t N = 100;
	double f_array[N];
	for (int i = 0; i < N; i++) {
		f_array[i] = i;
	}

	double f_low = N-1;
	int index = find_index_low(f_low, N, f_array);

	EXPECT_EQ(index, N-1);
}

TEST(find_index_low, not_exact) {
	size_t N = 100;
	double f_array[N];
	for (int i = 0; i < N; i++) {
		f_array[i] = i;
	}

	// f_array = [0, 1, 2, 3]
	double f = 1.1;

	// this should return the index representing 2, which is array index 2
	int index = find_index_low(f, N, f_array);

	EXPECT_EQ(index, 2);
}

TEST(find_index_low, not_found) {
	size_t N = 100;
	double f_array[N];
	for (int i = 0; i < N; i++) {
		f_array[i] = i;
	}

	double f_low = N;
	int index = find_index_low(f_low, N, f_array);

	EXPECT_EQ(index, -1);
}

TEST(find_index_high, left_end) {
	size_t N = 100;
	double f_array[N];
	for (int i = 0; i < N; i++) {
		f_array[i] = i;
	}

	double f = 0.0;
	int index = find_index_high(f, N, f_array);

	EXPECT_EQ(index, 0);
}

TEST(find_index_high, right_end) {
	size_t N = 100;
	double f_array[N];
	for (int i = 0; i < N; i++) {
		f_array[i] = i;
	}

	double f = N-1;
	int index = find_index_high(f, N, f_array);

	EXPECT_EQ(index, N-1);
}

TEST(find_index_high, not_exact) {
	size_t N = 100;
	double f_array[N];
	for (int i = 0; i < N; i++) {
		f_array[i] = i;
	}
	// f_array = [0, 1, 2, 3]

	double f = 2.3;
	int index = find_index_high(f, N, f_array);

	EXPECT_EQ(index, 2);
}

TEST(find_index_high, not_found) {
	size_t N = 100;
	double f_array[N];
	for (int i = 0; i < N; i++) {
		f_array[i] = i;
	}

	double f = N;
	int index = find_index_high(f, N, f_array);

	EXPECT_EQ(index, -1);
}

TEST(Detector_Antenna_Patterns_compute, valuesMatchMatlabVersion_H1) {
	// The input
	sky_t sky;
	sky.ra = 1.0;
	sky.dec = 1.0;
	double polarization_angle = 1.0;

	// we don't need the PSD, but the detector Init wants to use it.
	psd_t *psd = PSD_alloc(10);
	psd->type = PSD_ONE_SIDED;

	detector_antenna_patterns_workspace_t *ws = Detector_Antenna_Patterns_workspace_alloc();

	detector_antenna_patterns_t ap;

	detector_t *det = Detector_alloc();
	Detector_init(H1, psd, det);

	Detector_Antenna_Patterns_compute(det, &sky, polarization_angle, ws, &ap);

	ASSERT_NEAR( ap.u, -0.111648382531459, 1e-13);
	ASSERT_NEAR( ap.v, 0.241179796835088, 1e-13);
	ASSERT_NEAR( ap.f_plus, 0.265766289860558, 1e-13);
	ASSERT_NEAR( ap.f_cross, 0.001155377453101, 1e-13);

	//printf("%0.21e %0.21e %0.21e %0.21e\n", ap.u, ap.v, ap.f_plus, ap.f_cross);

	//PSD_free(psd) does not need to be called since the detector free's it's own psd
	Detector_free(det);
	Detector_Antenna_Patterns_workspace_free(ws);
}

TEST(Detector_Antenna_Patterns_compute, valuesMatchMatlabVersion_H2) {
	// The input
	sky_t sky;
	sky.ra = 1.0;
	sky.dec = 1.0;
	double polarization_angle = 1.0;

	// we don't need the PSD, but the detector Init wants to use it.
	psd_t *psd = PSD_alloc(10);
	psd->type = PSD_ONE_SIDED;

	detector_antenna_patterns_workspace_t *ws = Detector_Antenna_Patterns_workspace_alloc();

	detector_antenna_patterns_t ap;

	detector_t *det = Detector_alloc();
	Detector_init(H2, psd, det);

	Detector_Antenna_Patterns_compute(det, &sky, polarization_angle, ws, &ap);

	ASSERT_NEAR( ap.u, -0.111648382531459, 1e-13);
	ASSERT_NEAR( ap.v, 0.241179796835088, 1e-13);
	ASSERT_NEAR( ap.f_plus, 0.265766289860558, 1e-13);
	ASSERT_NEAR( ap.f_cross, 0.001155377453101, 1e-13);

	//PSD_free(psd) does not need to be called since the detector free's it's own psd
	Detector_free(det);
	Detector_Antenna_Patterns_workspace_free(ws);
}

TEST(Detector_Antenna_Patterns_compute, valuesMatchMatlabVersion_L1) {
	// The input
	sky_t sky;
	sky.ra = 1.0;
	sky.dec = 1.0;
	double polarization_angle = 1.0;

	// we don't need the PSD, but the detector Init wants to use it.
	psd_t *psd = PSD_alloc(10);
	psd->type = PSD_ONE_SIDED;


	detector_antenna_patterns_workspace_t *ws = Detector_Antenna_Patterns_workspace_alloc();

	detector_antenna_patterns_t ap;

	detector_t *det = Detector_alloc();
	Detector_init(L1, psd, det);

	Detector_Antenna_Patterns_compute(det, &sky, polarization_angle, ws, &ap);

	ASSERT_NEAR( ap.u, 0.082041190838257, 1e-13);
	ASSERT_NEAR( ap.v, -0.165392307790427, 1e-13);
	ASSERT_NEAR( ap.f_plus, -0.184531981924498, 1e-13);
	ASSERT_NEAR( ap.f_cross, -0.005772358046725, 1e-13);

	//PSD_free(psd) does not need to be called since the detector free's it's own psd
	Detector_free(det);
	Detector_Antenna_Patterns_workspace_free(ws);
}

TEST(Detector_Antenna_Patterns_compute, valuesMatchMatlabVersion_V1) {
	// The input
	sky_t sky;
	sky.ra = 1.0;
	sky.dec = 1.0;
	double polarization_angle = 1.0;

	// we don't need the PSD, but the detector Init wants to use it.
	psd_t *psd = PSD_alloc(10);
	psd->type = PSD_ONE_SIDED;


	detector_antenna_patterns_workspace_t *ws = Detector_Antenna_Patterns_workspace_alloc();

	detector_antenna_patterns_t ap;

	detector_t *det = Detector_alloc();
	Detector_init(V1, psd, det);

	Detector_Antenna_Patterns_compute(det, &sky, polarization_angle, ws, &ap);

	ASSERT_NEAR( ap.u, 0.339872953829318, 1e-13);
	ASSERT_NEAR( ap.v, -0.781006848074470, 1e-13);
	ASSERT_NEAR( ap.f_plus, -0.851604571851356, 1e-13);
	ASSERT_NEAR( ap.f_cross, 0.015967926783203, 1e-13);

	//PSD_free(psd) does not need to be called since the detector free's it's own psd
	Detector_free(det);
	Detector_Antenna_Patterns_workspace_free(ws);
}

TEST(Detector_Antenna_Patterns_compute, valuesMatchMatlabVersion_G1) {
	// The input
	sky_t sky;
	sky.ra = 1.0;
	sky.dec = 1.0;
	double polarization_angle = 1.0;

	// we don't need the PSD, but the detector Init wants to use it.
	psd_t *psd = PSD_alloc(10);
	psd->type = PSD_ONE_SIDED;


	detector_antenna_patterns_workspace_t *ws = Detector_Antenna_Patterns_workspace_alloc();

	detector_antenna_patterns_t ap;

	detector_t *det = Detector_alloc();
	Detector_init(G1, psd, det);

	Detector_Antenna_Patterns_compute(det, &sky, polarization_angle, ws, &ap);

	ASSERT_NEAR( ap.u, 0.760712553464735, 1e-13);
	ASSERT_NEAR( ap.v, 0.466049068159302, 1e-13);
	ASSERT_NEAR( ap.f_plus, 0.107209095805712, 1e-13);
	ASSERT_NEAR( ap.f_cross, -0.885658812809714, 1e-13);

	//PSD_free(psd) does not need to be called since the detector free's it's own psd
	Detector_free(det);
	Detector_Antenna_Patterns_workspace_free(ws);
}

TEST(Detector_Antenna_Patterns_compute, valuesMatchMatlabVersion_K1) {
	// The input
	sky_t sky;
	sky.ra = 1.0;
	sky.dec = 1.0;
	double polarization_angle = 1.0;

	// we don't need the PSD, but the detector Init wants to use it.
	psd_t *psd = PSD_alloc(10);
	psd->type = PSD_ONE_SIDED;


	detector_antenna_patterns_workspace_t *ws = Detector_Antenna_Patterns_workspace_alloc();

	detector_antenna_patterns_t ap;

	detector_t *det = Detector_alloc();
	Detector_init(K1, psd, det);

	Detector_Antenna_Patterns_compute(det, &sky, polarization_angle, ws, &ap);

	ASSERT_NEAR( ap.u, -0.642205332969477, 1e-13);
	ASSERT_NEAR( ap.v, 0.013696768146886, 1e-13);
	ASSERT_NEAR( ap.f_plus, 0.279706153760744, 1e-13);
	ASSERT_NEAR( ap.f_cross, 0.578255790027629, 1e-13);

	//PSD_free(psd) does not need to be called since the detector free's it's own psd
	Detector_free(det);
	Detector_Antenna_Patterns_workspace_free(ws);
}

TEST(Detector_Antenna_Patterns_compute, valuesMatchMatlabVersion_T1) {
	// The input
	sky_t sky;
	sky.ra = 1.0;
	sky.dec = 1.0;
	double polarization_angle = 1.0;

	// we don't need the PSD, but the detector Init wants to use it.
	psd_t *psd = PSD_alloc(10);
	psd->type = PSD_ONE_SIDED;


	detector_antenna_patterns_workspace_t *ws = Detector_Antenna_Patterns_workspace_alloc();

	detector_antenna_patterns_t ap;

	detector_t *det = Detector_alloc();
	Detector_init(T1, psd, det);

	Detector_Antenna_Patterns_compute(det, &sky, polarization_angle, ws, &ap);

	ASSERT_NEAR( ap.u, -0.377839806093768, 1e-13);
	ASSERT_NEAR( ap.v, -0.404490668062711, 1e-13);
	ASSERT_NEAR( ap.f_plus, -0.210565483616917, 1e-13);
	ASSERT_NEAR( ap.f_cross, 0.511896275360515, 1e-13);

	//PSD_free(psd) does not need to be called since the detector free's it's own psd
	Detector_free(det);
	Detector_Antenna_Patterns_workspace_free(ws);
}

TEST(Detector_time_delay, matchesMatlab_H1) {
	detector_t *d = Detector_alloc();
	psd_t *psd = PSD_alloc(10);
	psd->type = PSD_ONE_SIDED;

	Detector_init(H1, psd, d);

	sky_t sky;
	sky.ra = 1.0;
	sky.dec = 2.0;

	double td;
	Detector_time_delay(d, &sky, &td);

	Detector_free(d);

	ASSERT_NEAR( 0.020053494558775, td, 1e-9 );
}

TEST(Detector_time_delay, matchesMatlab_H2) {
	detector_t *d = Detector_alloc();
	psd_t *psd = PSD_alloc(10);
	psd->type = PSD_ONE_SIDED;

	Detector_init(H2, psd, d);

	sky_t sky;
	sky.ra = 1.0;
	sky.dec = 2.0;

	double td;
	Detector_time_delay(d, &sky, &td);

	Detector_free(d);

	ASSERT_NEAR( 0.020053494558775, td, 1e-9 );
}

TEST(Detector_time_delay, matchesMatlab_L1) {
	detector_t *d = Detector_alloc();
	psd_t *psd = PSD_alloc(10);
	psd->type = PSD_ONE_SIDED;

	Detector_init(L1, psd, d);

	sky_t sky;
	sky.ra = 1.0;
	sky.dec = 2.0;

	double td;
	Detector_time_delay(d, &sky, &td);

	Detector_free(d);

	ASSERT_NEAR( 0.016255155585154, td, 1e-9 );
}

TEST(Detector_time_delay, matchesMatlab_V1) {
	detector_t *d = Detector_alloc();
	psd_t *psd = PSD_alloc(10);
	psd->type = PSD_ONE_SIDED;

	Detector_init(V1, psd, d);

	sky_t sky;
	sky.ra = 1.0;
	sky.dec = 2.0;

	double td;
	Detector_time_delay(d, &sky, &td);

	Detector_free(d);

	ASSERT_NEAR( 0.008886162677520, td, 1e-9 );
}

TEST(Detector_time_delay, matchesMatlab_G1) {
	detector_t *d = Detector_alloc();
	psd_t *psd = PSD_alloc(10);
	psd->type = PSD_ONE_SIDED;

	Detector_init(G1, psd, d);

	sky_t sky;
	sky.ra = 1.0;
	sky.dec = 2.0;

	double td;
	Detector_time_delay(d, &sky, &td);

	Detector_free(d);

	ASSERT_NEAR( 0.011554151277148, td, 1e-9 );
}

TEST(Detector_time_delay, matchesMatlab_K1) {
	detector_t *d = Detector_alloc();
	psd_t *psd = PSD_alloc(10);
	psd->type = PSD_ONE_SIDED;

	Detector_init(K1, psd, d);

	sky_t sky;
	sky.ra = 1.0;
	sky.dec = 2.0;

	double td;
	Detector_time_delay(d, &sky, &td);

	Detector_free(d);

	ASSERT_NEAR( 0.010187890845916, td, 1e-9 );
}

TEST(Detector_time_delay, matchesMatlab_T1) {
	detector_t *d = Detector_alloc();
	psd_t *psd = PSD_alloc(10);
	Detector_init(T1, psd, d);

	sky_t sky;
	sky.ra = 1.0;
	sky.dec = 2.0;

	double td;
	Detector_time_delay(d, &sky, &td);

	Detector_free(d);

	ASSERT_NEAR( 0.010247681186132, td, 1e-9 );
}



TEST(ChirpTime, matchesMatlab) {
	// Inputs needed to compute the chirp times and other values
	double m1 = 1.4 * GSL_CONST_MKSA_SOLAR_MASS;
	double m2 = 3.0 * GSL_CONST_MKSA_SOLAR_MASS;
	double time_of_arrival = 2.2;
	double f_low = 70.0;

	inspiral_chirp_factors_t cf;
	CF_compute_for_signal(f_low, m1, m2, time_of_arrival, &cf);
	//CF_print(&cf);

	ASSERT_NEAR( cf.total_mass, 8.751247999999999e+30, 1e23 );
	ASSERT_NEAR( cf.reduced_mass, 1.898514545454545e+30, 1e23 );
	ASSERT_NEAR( cf.chirp_mass, 3.498455498072740e+30, 1e23 );
	ASSERT_NEAR( cf.s_mass_ratio, 0.216942148760331, 1e-9 );
	ASSERT_NEAR( cf.multi_fac, 0.004766251118327, 1e-9 );
	ASSERT_NEAR( cf.calculated_reduced_mass, 1.898514545454546e+30, 1e23 );
	ASSERT_NEAR( cf.calculated_total_mass, 8.751247999999995e+30, 1e23 );
	ASSERT_NEAR( cf.t_chirp, 3.085046935756838, 1e-9 );
	ASSERT_NEAR( cf.s_mass_ratio_cal, 0.216942148760331, 1e-9 );
	ASSERT_NEAR( cf.multi_fac_cal, 0.004766251118327, 1e-9 );
	ASSERT_NEAR( cf.ct.chirp_time0, 3.032796861027544, 1e-9 );
	ASSERT_NEAR( cf.ct.chirp_time1_5, 0.290636455764961, 1e-9 );
	ASSERT_NEAR( cf.ct.chirp_time1, 0.321574578546665, 1e-9 );
	ASSERT_NEAR( cf.ct.chirp_time2, 0.021311951947589, 1e-9 );
}

TEST(SP_workspace, frequencies_and_indices) {
	size_t N = 10;
	asd_t *asd = ASD_alloc( N );
	for (int i = 0; i < N; i++) {
		asd->f[i] = i;
		asd->asd[i] = 1.0;
		asd->type = ASD_ONE_SIDED;
	}

	double f_low = 2.0;
	double f_high = 6.0;

	stationary_phase_workspace_t *sp_workspace = SP_workspace_alloc(f_low, f_high, asd->len, asd->f);

	EXPECT_EQ( sp_workspace->f_low_index, 2);
	EXPECT_EQ( sp_workspace->f_high_index, 6);
	EXPECT_EQ( sp_workspace->f_low, f_low);
	EXPECT_EQ( sp_workspace->f_high, f_high);
	EXPECT_EQ( sp_workspace->len, 5);

	SP_workspace_free(sp_workspace);
	ASD_free(asd);
}

TEST(SP_workspace, coefficients) {
	size_t N = 10;
	asd_t *asd = ASD_alloc( N );
	for (int i = 0; i < N; i++) {
		asd->f[i] = i;
		asd->asd[i] = 1.0;
	}

	double f_low = 1.0;
	double f_high = 6.0;

	stationary_phase_workspace_t *sp_workspace = SP_workspace_alloc(f_low, f_high, asd->len, asd->f);

	for (int i = 0; i < asd->len; i++) {
		asd->f[i] = 1.0;
	}

	SP_workspace_init( asd->len, asd->f, sp_workspace );

	// These number are computing using f_low = 1.0 which affects f_fac
	for (int i = 0; i < sp_workspace->len; i++) {
		EXPECT_EQ( sp_workspace->g_coeff[i],  1.0);
		EXPECT_EQ( sp_workspace->chirp_tc_coeff[i], 2.0 * M_PI );
		EXPECT_EQ( sp_workspace->constant_coeff[i], -M_PI / 4.0 );
		EXPECT_EQ( sp_workspace->chirp_time_0_coeff[i], 2.0 * M_PI * 3.0 / 5.0 );
		EXPECT_EQ( sp_workspace->chirp_time_1_coeff[i],  2.0 * M_PI );
		EXPECT_EQ( sp_workspace->chirp_time1_5_coeff[i], -3.0 * M_PI );
		EXPECT_EQ( sp_workspace->chirp_time2_coeff[i], 6.0 * M_PI );
	}

	EXPECT_EQ( sp_workspace->f_low_index, 1);
	EXPECT_EQ( sp_workspace->f_high_index, 6);
	EXPECT_EQ( sp_workspace->f_low, f_low);
	EXPECT_EQ( sp_workspace->f_high, f_high);
	EXPECT_EQ( sp_workspace->len, 6);

	SP_workspace_free(sp_workspace);
	ASD_free(asd);
}

TEST(SP_compute, valuesMatcheMatlabVersion) {
	double detector_time_delay = 1.0;
	double detector_normalization_factor = 2.0;
	double inspiral_coalesce_phase = 3.0;
	inspiral_chirp_time_t ct;
	ct.chirp_time0 = 4.0;
	ct.chirp_time1 = 5.0;
	ct.chirp_time1_5 = 6.0;
	ct.chirp_time2 = 7.0;
	ct.tc = 8.0;
	double f_low = 1.0;
	double f_high = 6.0;

	size_t len_f_array = 10;
	double f_array[10];
	for (int i = 0; i < len_f_array; i++) {
		f_array[i] = i;
	}

	stationary_phase_workspace_t *w = SP_workspace_alloc(f_low, f_high, len_f_array, f_array);
	stationary_phase_t *s = SP_alloc(10);
	SP_compute(detector_time_delay, detector_normalization_factor,
			inspiral_coalesce_phase, &ct,
			w,
			s);

	// These values were obtained from the Matlab program
	gsl_complex matlab_values[10];
	GSL_SET_COMPLEX(&matlab_values[0], 0.0, 0.0);
	GSL_SET_COMPLEX(&matlab_values[1], -0.213089577446166, -0.452319391562880);
	GSL_SET_COMPLEX(&matlab_values[2], 0.001412558668514, -0.222720200143620);
	GSL_SET_COMPLEX(&matlab_values[3], -0.074184098785065, -0.117289193389998);
	GSL_SET_COMPLEX(&matlab_values[4], -0.037541561157106, -0.091835529008064);
	GSL_SET_COMPLEX(&matlab_values[5], -0.052649099014614, 0.055462670773910);
	GSL_SET_COMPLEX(&matlab_values[6], 0.043808040396736, -0.043618008358807);
	GSL_SET_COMPLEX(&matlab_values[7], 0.0, 0.0);
	GSL_SET_COMPLEX(&matlab_values[8], 0.0, 0.0);
	GSL_SET_COMPLEX(&matlab_values[9], 0.0, 0.0);

	for (int i = 0; i < s->len; i++) {
		ASSERT_NEAR( GSL_REAL(s->spa_0[i]), GSL_REAL(matlab_values[i]), 1e-13);
		ASSERT_NEAR( GSL_IMAG(s->spa_0[i]), GSL_IMAG(matlab_values[i]), 1e-13);
	}

	SP_free(s);
	SP_workspace_free(w);
}

TEST(SP_normalization, valuesMatchMatlabVersion) {
	double f_low = 1.1;
	double f_high = 9.0;

	size_t len_f_array = 10;

	asd_t *asd = ASD_alloc( len_f_array );
	for (int i = 0; i < asd->len; i++) {
		asd->f[i] = i;
		asd->asd[i] = i;
		asd->type = ASD_ONE_SIDED;
	}

	stationary_phase_workspace_t *w = SP_workspace_alloc(f_low, f_high, asd->len, asd->f);

	double g = SP_normalization_factor(asd, w);
	//printf("normalization factor = %0.21e\n", g);

	ASSERT_NEAR( g, 1.436106008421560, 1e-13 );

	SP_workspace_free(w);
	ASD_free(asd);
}

TEST(coherent_network_statistic, CN_do_work) {
	size_t i;
	size_t num_time_samples = 10;
	size_t half_size = 6;
	size_t f_low_index = 1;
	size_t f_high_index = 3;

	asd_t *asd = ASD_alloc( half_size );
	asd->type = ASD_ONE_SIDED;
	for (i = 0; i < half_size; i++ ) {
		asd->f[i] = i;
	}
	asd->asd[0] = 1;
	asd->asd[1] = 5;
	asd->asd[2] = 10;
	asd->asd[3] = 15;
	asd->asd[4] = 5;
	asd->asd[5] = 2;

	gsl_complex *spa = (gsl_complex*) malloc( half_size * sizeof(gsl_complex) );
	spa[0] = gsl_complex_rect(1, -2);
	spa[1] = gsl_complex_rect(2, 3);
	spa[2] = gsl_complex_rect(5, -2);
	spa[3] = gsl_complex_rect(1, -1);
	spa[4] = gsl_complex_rect(0, -1);
	spa[5] = gsl_complex_rect(2, -3);

	gsl_complex *half_fft_data = (gsl_complex*) malloc( half_size * sizeof(gsl_complex) );
	half_fft_data[0] = gsl_complex_rect(0, 7);
	half_fft_data[1] = gsl_complex_rect(1, -2);
	half_fft_data[2] = gsl_complex_rect(4, 6);
	half_fft_data[3] = gsl_complex_rect(4, 4);
	half_fft_data[4] = gsl_complex_rect(7, -7);
	half_fft_data[5] = gsl_complex_rect(3, -2);

	gsl_complex *temp = (gsl_complex*) malloc( half_size * sizeof(gsl_complex) );
	for (i = 0; i < half_size; i++) {
		temp[i] = gsl_complex_rect(0.0, 0.0);
	}

	gsl_complex *out_c = (gsl_complex*) malloc( num_time_samples * sizeof(gsl_complex) );
	for (i = 0; i < num_time_samples; i++) {
		out_c[i] = gsl_complex_rect(0.0, 0.0);
	}

	CN_do_work(num_time_samples, f_low_index, f_high_index, spa,
			asd, half_fft_data, temp, out_c);

	/*for (i = 0; i < num_time_samples; i++) {
		printf("%0.21e \t %0.21e\n", GSL_REAL(out_c[i]), GSL_IMAG(out_c[i]));
	}*/
	ASSERT_NEAR( GSL_REAL(out_c[0]), -14.0, 1e-9);
	ASSERT_NEAR( GSL_IMAG(out_c[0]), 7.0, 1e-9);

	ASSERT_NEAR( GSL_REAL(out_c[1]), -0.80, 1e-9);
	ASSERT_NEAR( GSL_IMAG(out_c[1]), -1.4, 1e-9);

	ASSERT_NEAR( GSL_REAL(out_c[2]), 0.8, 1e-9);
	ASSERT_NEAR( GSL_IMAG(out_c[2]), 3.8, 1e-9);

	ASSERT_NEAR( GSL_REAL(out_c[3]), 0.0, 1e-9);
	ASSERT_NEAR( GSL_IMAG(out_c[3]), 0.533333333333333, 1e-9);

	ASSERT_NEAR( GSL_REAL(out_c[4]), 1.4, 1e-9);
	ASSERT_NEAR( GSL_IMAG(out_c[4]), 1.4, 1e-9);

	ASSERT_NEAR( GSL_REAL(out_c[5]), 6.0, 1e-9);
	ASSERT_NEAR( GSL_IMAG(out_c[5]), 2.5, 1e-9);

	ASSERT_NEAR( GSL_REAL(out_c[6]), 1.4, 1e-9);
	ASSERT_NEAR( GSL_IMAG(out_c[6]), -1.4, 1e-9);

	ASSERT_NEAR( GSL_REAL(out_c[7]), 0.0, 1e-9);
	ASSERT_NEAR( GSL_IMAG(out_c[7]), -0.533333333333333, 1e-9);

	ASSERT_NEAR( GSL_REAL(out_c[8]), 0.8, 1e-9);
	ASSERT_NEAR( GSL_IMAG(out_c[8]), -3.8, 1e-9);

	ASSERT_NEAR( GSL_REAL(out_c[9]), -0.8, 1e-9);
	ASSERT_NEAR( GSL_IMAG(out_c[9]), 1.4, 1e-9);

	ASD_free(asd);
	free(spa);
	free(half_fft_data);
	free(temp);
	free(out_c);
}

TEST(coherent_network_statistic, CN_compute_valuesMatchMatlabVersion) {
	sky_t sky;
	sky.ra = 1.0;
	sky.dec = 1.0;

	//double polarization_angle = 1.0;

	inspiral_chirp_time_t ct;
	ct.chirp_time0 = 4.0;
	ct.chirp_time1 = 5.0;
	ct.chirp_time1_5 = 6.0;
	ct.chirp_time2 = 7.0;
	ct.tc = ct.chirp_time0 + ct.chirp_time1 - ct.chirp_time1_5 + ct.chirp_time2;

	double f_low = 4.0;
	double f_high = 4.0;

	size_t num_detectors = 4;

	size_t num_time_samples = 10;

	// Generate data
	network_strain_half_fft_t *network_strain = network_strain_half_fft_alloc(
			num_detectors, num_time_samples);
	for (int i = 0; i < num_detectors; i++) {
		for (int k = 0; k < network_strain->strains[i]->half_fft_len; k++) {
			network_strain->strains[i]->half_fft[k] = gsl_complex_rect(k, k);
		}
	}

	size_t len_f_array = network_strain->strains[0]->half_fft_len;
	//fprintf(stderr, "len_f_array = %lu\n", len_f_array);

	detector_network_t *net = Detector_Network_alloc( num_detectors );
	DETECTOR_ID ids[4] = {H1,L1,V1,K1};
	for (int i = 0; i < num_detectors; i++) {
		psd_t *psd = PSD_alloc(len_f_array);
		for (int k = 0; k < len_f_array; k++) {
			psd->f[k] = k;
			psd->psd[k] = 1.0;
			psd->type = PSD_ONE_SIDED;
		}
		Detector_init(ids[i], psd, net->detector[i]);
	}


	double network_snr;

	coherent_network_workspace_t *ws = CN_workspace_alloc(
			num_time_samples, net, len_f_array, f_low, f_high);

	coherent_network_statistic(
			net,
			f_low,
			f_high,
			&ct,
			&sky,
			network_strain,
			ws,
			&network_snr,
			NULL);

	/*
	for (int i = 0; i < num_detectors; i++) {
		for (int j = 0; j < num_time_samples; j++) {
			gsl_complex z = ws->helpers[i]->c_plus[j];
		    fprintf(stderr, "Detector %d: c_plus = %0.21e %0.21e\n",
		    		i, GSL_REAL(z), GSL_IMAG(z));
		}
		fprintf(stderr, "\n\n");
	}*/
	gsl_complex z = ws->helpers[0]->c_plus[0];
	EXPECT_NEAR( GSL_REAL(z), 0.0, 1e-12 );
	EXPECT_NEAR( GSL_IMAG(z), 0.0, 1e-12 );

	z = ws->helpers[0]->c_plus[4];
	EXPECT_NEAR( GSL_REAL(z), -0.438120135833806, 1e-12 );
	EXPECT_NEAR( GSL_IMAG(z), -0.240969309587590, 1e-12 );

	z = ws->helpers[0]->c_plus[6];
	EXPECT_NEAR( GSL_REAL(z), -0.438120135833806, 1e-12 );
	EXPECT_NEAR( GSL_IMAG(z), 0.240969309587590, 1e-12 );

	EXPECT_NEAR( network_snr, 1.307720293350521867382, 1e-12);

	//fprintf(stderr, "network snr = %0.21e\n", network_snr);

	CN_workspace_free(ws);

	Detector_Network_free(net);

	network_strain_half_fft_free(network_strain);

}

#endif

