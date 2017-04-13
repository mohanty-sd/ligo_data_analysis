#include <assert.h>
#include <stdio.h>

#include <gsl/gsl_math.h>
#include <gsl/gsl_vector.h>
#include <gsl/gsl_errno.h>
#include <gsl/gsl_rng.h>

#include "ptapso.h"
#include "ptapso_maxphase.h"

#include "inspiral_pso_fitness.h"
#include "inspiral_chirp_time.h"
#include "random.h"
#include "sky.h"

#include "settings_file.h"

pso_fitness_function_parameters_t* pso_fitness_function_parameters_alloc(
		double f_low, double f_high, detector_network_t* network, network_strain_half_fft_t *network_strain) {
	pso_fitness_function_parameters_t *params = (pso_fitness_function_parameters_t*) malloc( sizeof(pso_fitness_function_parameters_t) );
	if (params == NULL) {
		fprintf(stderr, "Error. Unable to allocate memory for the pso_fitness_function_parameters_t. Aborting.\n");
		abort();
	}

	coherent_network_workspace_t *workspace = CN_workspace_malloc(
			network_strain->num_time_samples, network, network->detector[0]->asd->len,
			f_low, f_high);

	/* Setup the parameter structure for the pso fitness function */
	params->f_low = f_low;
	params->f_high = f_high;
	params->network = network;
	params->network_strain = network_strain;
	params->workspace = workspace;

	return params;
}

void pso_fitness_function_parameters_free(pso_fitness_function_parameters_t *params) {
	assert(params != NULL);
	CN_workspace_free(params->workspace);
	free(params);
	params = NULL;
}

double pso_fitness_function(gsl_vector *xVec, void  *inParamsPointer){

	unsigned int validPt;
    unsigned int lpc;
	//! [Cast fit func params]
	struct fitFuncParams *inParams = (struct fitFuncParams *)inParamsPointer;
	//! [Cast fit func params]
	unsigned int ncols = inParams->nDim;
	double rangeVec, rmin, x;
	double fitFuncVal;
	//! [Cast special params]
	/* Shows how to retrieve special parameters (dummy ones are supplied for this particular
	fitness function).
	*/
	struct pso_fitness_function_parameters_s *splParams = (struct pso_fitness_function_parameters_s *)inParams->splParams;

	/* This fitness function knows what fields are given in the special parameters struct */

	s2rvector(xVec,inParams->rmin,inParams->rangeVec,inParams->realCoord);

	validPt = chkstdsrchrng(xVec);

	if (validPt){
		inParams->fitEvalFlag = 1;
		fitFuncVal = 0;

		double ra = gsl_vector_get(inParams->realCoord, 0);
		double dec = gsl_vector_get(inParams->realCoord, 1);
		double chirp_time_0 = gsl_vector_get(inParams->realCoord, 2);
		double chirp_time_1_5 = gsl_vector_get(inParams->realCoord, 3);

		inspiral_chirp_time_t chirp_time;
		CF_CT_compute(splParams->f_low, chirp_time_0, chirp_time_1_5, &chirp_time);

		/* The network statistic requires the time of arrival to be zero
		   in order for the matched filtering to work correctly. */


		sky_t sky;
		sky.ra = ra;
		sky.dec = dec;

		/* DANGER */
		double polarization_angle = 0.0;

		coherent_network_statistic(
				splParams->network,
				splParams->f_low,
				splParams->f_high,
				&chirp_time,
				&sky,
				polarization_angle,
				splParams->network_strain,
				splParams->workspace,
				&fitFuncVal);
		/* The statistic is larger for better matches, but PSO is finding
		   minimums, so multiply by -1.0. */
		fitFuncVal *= -1.0;
    }
	else{
		fitFuncVal=GSL_POSINF;
		inParams->fitEvalFlag = 0;
	}
   return fitFuncVal;
}

int pso_estimate_parameters(char *pso_settings_filename, pso_fitness_function_parameters_t *splParams, gslseed_t seed, pso_result_t* result) {
	/* Estimate right-ascension and declination */
	unsigned int nDim = 4, lpc;
	/* [0] = RA
	   [1] = Declination
	   [2] = Chirp time 0
	   [3] = Chirp time 1.5
	 */
	/* Shihan said his PSO went from 0 to PI for dec */
	double rmin[4] = {-M_PI, 	-0.5*M_PI, 	0.0, 		0.0};
	double rmax[4] = {M_PI, 	0.5*M_PI, 	43.4673, 	1.0840};
	double rangeVec[4];

	/* Error handling off */
	gsl_error_handler_t *old_handler = gsl_set_error_handler_off ();

	/* Initialize random number generator */
	gsl_rng *rngGen = gsl_rng_alloc(gsl_rng_taus);
	/* Soumya version gsl_rng_set(rngGen,2571971); */
	gsl_rng_set(rngGen, seed);

    /* Allocate fitness function parameter struct.
	 */
	struct fitFuncParams *inParams = ffparam_alloc(nDim);

	/* Load fitness function parameter struct */
	for (lpc = 0; lpc < nDim; lpc++){
		rangeVec[lpc]=rmax[lpc]-rmin[lpc];
		gsl_vector_set(inParams->rmin,lpc,rmin[lpc]);
		gsl_vector_set(inParams->rangeVec,lpc,rangeVec[lpc]);
	}
	/* Set up pointer to fitness function. Use the prototype
	declaration given in the header file for the fitness function. */
	double (*fitfunc)(gsl_vector *, void *) = pso_fitness_function;
	/* Set up special parameters, if any, needed by the fitness function used.
	   These should be provided in a structure that should be defined in
	   the fitness function's header file.
	 */
	/* ============================================ */
	/* Pass on the special parameters through the generic fitness function parameter
	struct */
	inParams->splParams = splParams;

	/* Set up storage for output from ptapso. */
	struct returnData *psoResults = returnData_alloc(nDim);


	/* nelder-meade method .. look up */
	/* Set up the pso parameter structure.*/

	/* Load the pso settings */
	settings_file_t *settings_file = settings_file_open(pso_settings_filename);
	if (settings_file == NULL) {
		printf("Error opening the PSO settings file (%s). Aborting.\n", pso_settings_filename);
		abort();
	}

	struct psoParamStruct psoParams;
	psoParams.popsize = atoi(settings_file_get_value(settings_file, "popsize"));;
	psoParams.maxSteps= atoi(settings_file_get_value(settings_file, "maxSteps"));;
	psoParams.c1 = atof(settings_file_get_value(settings_file, "c1"));;
	psoParams.c2 = atof(settings_file_get_value(settings_file, "c2"));;
	psoParams.max_velocity = atof(settings_file_get_value(settings_file, "max_velocity"));;
	psoParams.dcLaw_a = atof(settings_file_get_value(settings_file, "dcLaw_a"));;
	psoParams.dcLaw_b = atof(settings_file_get_value(settings_file, "dcLaw_b"));;
	psoParams.dcLaw_c = psoParams.maxSteps;
	psoParams.dcLaw_d = atof(settings_file_get_value(settings_file, "dcLaw_d"));;
	psoParams.locMinIter = atof(settings_file_get_value(settings_file, "locMinIter"));
	psoParams.locMinStpSz = atof(settings_file_get_value(settings_file, "locMinStpSz"));
	psoParams.rngGen = rngGen;
	psoParams.debugDumpFile = NULL; /*fopen("ptapso_dump.txt","w"); */

	settings_file_close(settings_file);

	ptapso(nDim, fitfunc, inParams, &psoParams, psoResults);

	/* convert values to function ranges, instead of pso ranges */
	s2rvector(psoResults->bestLocation,inParams->rmin,inParams->rangeVec,inParams->realCoord);
	result->ra = gsl_vector_get(inParams->realCoord, 0);
	result->dec = gsl_vector_get(inParams->realCoord, 1);
	result->chirp_t0 = gsl_vector_get(inParams->realCoord, 2);
	result->chirp_t1_5 = gsl_vector_get(inParams->realCoord, 3);
	result->snr = -1.0 * psoResults->bestFitVal;

	/* Free allocated memory */
	ffparam_free(inParams);
	returnData_free(psoResults);
	gsl_rng_free(rngGen);

	return 0;
}