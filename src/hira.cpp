#include <Rcpp.h>
#include <algorithm>  // Include the C++ standard library for the sort function
#include <cmath>
#include "spatial_shared.h"
using namespace Rcpp;






float TS(float a, float b, float c) {
  float denum = a + b + c;

  float ts = denum > 0 ? a / denum : 0;

}

// [[Rcpp::export]]
Rcpp::List get_hira_basic_scores(NumericVector obsvect, NumericMatrix indices, NumericMatrix fcfield,
           NumericVector scales) {

	// indices is Nx2 matrix the location of the grid point in the model passed grid.
    // indices(0,:) is the first index and indices(1,:) is the second index of fcfield which represents the model grid.


    int n_scales = scales.length();
    int ni = fcfield.nrow(), nj = fcfield.ncol();
    int no = obsvect.length();



    NumericMatrix cum_fc(ni, nj);
    NumericVector sum_fc(no);

    //Rcout << "dims: " << cum_bin_fc.nrow() << " " << cum_bin_fc.ncol() << "\n";



    // numeric vectors for the result
    NumericVector res_basic_size(n_scales);



    // Basic
    NumericVector res_basic_bias(n_scales);
    NumericVector res_basic_mae(n_scales);
    NumericVector res_basic_mse(n_scales);
    NumericVector res_basic_count(n_scales);



	cum_fc = cumsum2d(fcfield);


    int k = 0;

    for (int sc = 0; sc < n_scales; sc++) {



         int rad = (int) scales[sc];
         float norm = 1./((2 * rad + 1) * (2 * rad + 1));




        //Rcout << "dims: " << n_scales << " " << k;
        res_basic_size(k) = scales(sc);
        sum_fc = window_sum_from_cumsum_for_ij(cum_fc, rad, indices);


        res_basic_count[k] = no;
        res_basic_bias[k] = 0;
        res_basic_mae[k] = 0;
        res_basic_mse[k] = 0;

        for (int j = 0; j < no; j++) {
           float ff = sum_fc[j]*norm;
           res_basic_bias[k] += obsvect[j] - ff;
           res_basic_mae[k]  += abs(obsvect[j] - ff);
           res_basic_mse[k]  += (obsvect[j] - ff)*(obsvect[j] - ff);

        }
        if (no>0) {
          res_basic_bias[k] /= no;
          res_basic_mae[k]  /= no;
          res_basic_mse[k]  /= no;
        } else {

          res_basic_bias[k] = -9999; //Rcpp::NumericVector::get_na();
          res_basic_mae[k]  = -9999; //Rcpp::NumericVector::get_na();
          res_basic_mse[k]  = -9999; //Rcpp::NumericVector::get_na();
        }


        k++;

    } // sc



    Rcpp::List resultList;

	Rcpp::DataFrame df = Rcpp::DataFrame::create(
	  Named("scale") = res_basic_size,
	  Named("count") = res_basic_count,
	  Named("bias") =  res_basic_bias);
      resultList["hira_bias"] = df;

    df = Rcpp::DataFrame::create(
	  Named("scale") = res_basic_size,
	  Named("count") = res_basic_count,
	  Named("mae") =  res_basic_mae);
	  resultList["hira_mae"] = df;

    df = Rcpp::DataFrame::create(
	  Named("scale") = res_basic_size,
	  Named("count") = res_basic_count,
	  Named("mse") =  res_basic_mse);
	  resultList["hira_mse"] = df;



  return resultList;
}


// [[Rcpp::export]]
Rcpp::List get_hira_scores(NumericVector obsvect, NumericMatrix indices, NumericMatrix fcfield,NumericVector thresholds,
NumericVector scales, NumericVector strategies) {
  // indices is Nx2 matrix the location of the grid point in the model passed grid.
  // indices(0,:) is the first index and indices(1,:) is the second index of fcfield which represents the model grid.



  int n_thresholds = thresholds.length();
  int n_scales = scales.length();
  int ni = fcfield.nrow(), nj = fcfield.ncol();
  int no = obsvect.length();

  int nstrat = strategies.length(); // Number of Strategies

  NumericVector res_count(n_thresholds * n_scales);

  NumericVector sum_bin_fc(no);
  NumericVector sum_bin_ob(no);
  IntegerVector bin_ob(no);
  NumericMatrix cum_bin_fc(ni, nj);
  NumericMatrix cum_bin_ob(ni, nj);
  NumericMatrix obsongrid(ni, nj);

  //NumericMatrix cum_fc(ni, nj);
  //NumericVector sum_fc(no);

  //Rcout << "dims: " << cum_bin_fc.nrow() << " " << cum_bin_fc.ncol() << "\n";



  // numeric vectors for the result
  NumericVector res_thresh(n_thresholds * n_scales);
  NumericVector res_size(n_thresholds * n_scales);
  //NumericVector res_basic_size(n_scales);

  // Multi Event
  NumericVector res_me_a(n_thresholds * n_scales);
  NumericVector res_me_b(n_thresholds * n_scales);
  NumericVector res_me_c(n_thresholds * n_scales);
  NumericVector res_me_d(n_thresholds * n_scales);
  // Pragramtic
  NumericVector res_pra_bss(n_thresholds * n_scales);
  NumericVector res_pra_bs(n_thresholds * n_scales);
  //Conditional Square Root RPS

  // Theat Detection by Daniel
  NumericVector res_td_a(n_thresholds * n_scales);
  NumericVector res_td_b(n_thresholds * n_scales);
  NumericVector res_td_c(n_thresholds * n_scales);
  NumericVector res_td_d(n_thresholds * n_scales);

  // Basic
  //NumericVector res_basic_bias(n_scales);
  //NumericVector res_basic_mae(n_scales);
  //NumericVector res_basic_mse(n_scales);
  //NumericVector res_basic_count(n_scales);


  // NumericMatrix res_cdf(n_thresholds , n_scales);
  //NumericVector res_csrr_pre_rps(n_thresholds * n_scales);
  //NumericVector res_csrr_pre_px(n_thresholds * n_scales);
  NumericVector res_csrr_py(n_thresholds*n_scales*no); // first one corrsponds to th = 0 mm
  NumericVector res_csrr_px(n_thresholds*n_scales*no);

  bool is_multi_event = false; // 0
  bool is_pragmatic = false; // 1
  bool is_csrr = false; // 2 Conditional square root for RPS
  bool is_td = false; // 3 Practically Perfect Hindcast

  //bool is_basic = false; // 4 basic scores, bias, mse , mae

  for (int is = 0; is < nstrat; is++) {
    int stra = (int) strategies(is);

    switch (stra) {
    case 0:
      is_multi_event = true;
      break;
    case 1:
      // Pragmatic Stratigy
      is_pragmatic = true;
      break;
    case 2:
      is_csrr = true;
      break;
    case 3:
      is_td = true;
      break;
    //case 4:
    //  is_basic = true;
    //  break;
      //default:
      // code block
    }
  }

  //if (is_basic) {
  //cum_fc = cumsum2d(fcfield);
  //}


  if (is_td) {
    for (int j=0 ; j < nj ; j++) {
      for (int i=0 ; i < ni ; i++) {
        obsongrid(i,j) = 0;
      }
      }
    for (int j = 0; j < no; j++) {
      obsongrid(indices(j,0),indices(j,1)) = obsvect[j];
    }

  }

  //bool basic_is_done = !is_basic;
  //Rcout << "dims: basic_is_done " << basic_is_done;
  int k = 0;
  // Go down fro threshoulds for the csrr score and other scores based on CDF
  for (int th = 0 ; th < n_thresholds ; th++) {

    bin_ob = vector_to_bin(obsvect, thresholds[th]);
    cum_bin_fc = cumsum2d_bin(fcfield, thresholds[th]);

	if (is_td) {
      cum_bin_ob = cumsum2d_bin(obsongrid, thresholds[th]);
    }

    for (int sc = 0; sc < n_scales; sc++) {

      res_thresh(k) = thresholds(th);
      res_size(k) = scales(sc);

      int rad = (int) scales[sc];
      float norm = 1./((2 * rad + 1) * (2 * rad + 1));


      sum_bin_fc = window_sum_from_cumsum_for_ij(cum_bin_fc, rad, indices);

	  if (is_td) {
	     sum_bin_ob = window_sum_from_cumsum_for_ij(cum_bin_ob, rad, indices);
      }

      //if (!basic_is_done) {
	  //  Rcout << "dims: " << n_scales << " " << k;
	  //  res_basic_size(k) = scales(sc);
	  //  sum_fc = window_sum_from_cumsum_for_ij(cum_fc, rad, indices);
	  //
	  //
      //    res_basic_count[k] = no;
      //    res_basic_bias[k] = 0;
      //    res_basic_mae[k] = 0;
      //    res_basic_mse[k] = 0;
	  //
	  //  for (int j = 0; j < no; j++) {
	  //     float ff = sum_fc[j]*norm;
      //       res_basic_bias[k] += obsvect[j] - ff;
      //       res_basic_mae[k]  += abs(obsvect[j] - ff);
      //       res_basic_mse[k]  += (obsvect[j] - ff)*(obsvect[j] - ff);
	  //
	  //  }
	  //  res_basic_bias[k] /= no;
	  //  res_basic_mae[k]  /= no;
	  //  res_basic_mse[k]  /= no;
	  //}

      if (is_multi_event) {


        res_me_a[k] = 0;
        res_me_b[k] = 0;
        res_me_c[k] = 0;
        for (int j = 0; j < no; j++) {
          //TODO: It could be esier to use only the bitwise operations. the empysise of logical values to is only just to make sure
          bool is_fc = sum_bin_fc[j] > 0;
          bool is_obs = bin_ob[j] > 0;
		  //Rcout <<"is_fc: " << ((is_fc)) << "\n";
		  //Rcout <<"bin_ob: " << (is_obs) << "\n";
		  //Rcout <<"a: " << ((is_obs) && (is_fc)) << "\n";
		  //Rcout <<"b: " << ((!is_obs) && (is_fc)) << "\n";
		  //Rcout <<"c: " << ((is_obs) && (!is_fc)) << "\n";

          res_me_a[k] += (int)((is_obs) && (is_fc));
          res_me_b[k] += (int)((!is_obs) && (is_fc));
          res_me_c[k] += (int)((is_obs) && (!is_fc));

        }
         res_me_d[k] = no - res_me_a[k] - res_me_b[k] - res_me_c[k];
      }
      //Rcpp::stop("Stopping R execution from C++ code");
      if (is_pragmatic) {
        float nume = 0;
        float px_ave = 0;
        for (int j = 0; j < no; j++) {
          float diff = sum_bin_fc(j)*norm - (bin_ob[j]);
          nume += diff * diff;
          px_ave += bin_ob[j];
        }
        px_ave = px_ave / no;

        float denume = 0;
        for (int j = 0; j < no; j++) {
          float diff = px_ave - (bin_ob[j]);
          denume += diff * diff;
        }

        res_pra_bss[k] = denume  > 0 ? 1 - nume / denume : -9999; // Rcpp::NumericVector::get_na();

        res_pra_bs[k] = no > 0 ? nume / no:  -9999; //Rcpp::NumericVector::get_na();

      }

      if (is_td) {
        // Practically Perfect hindcast
        // Here I implement a slightly modified method.
        // Look for all grid points that fall in the same neighborhood of some scale.


        res_td_a[k] = 0;
        res_td_b[k] = 0;
        res_td_c[k] = 0;
        res_td_d[k] = 0;
        for(int iob=0; iob<no;iob++){
          int co = sum_bin_ob(iob);
          int cf = sum_bin_fc(iob);
          res_td_a[k] += (int) (co > 0 && cf >= co);
          res_td_b[k] += (int) (co == 0 && cf !=0);
          res_td_c[k] += (int) (co > 0 && cf < co);
        }

        res_td_d[k] = no - res_td_a[k] - res_td_b[k] - res_td_c[k];

      }

      if (is_csrr) {
        for (int j = 0; j < no; j++) {
		      res_csrr_px(th * n_scales * no + sc * no + j) = (bin_ob[j]);
          res_csrr_py(th * n_scales * no + sc * no + j) = sum_bin_fc[j] * norm;
        }
      }

	    res_count[k] = no;
      k++;

    } // sc
	//basic_is_done = true;
  } // th

   Rcpp::List resultList;


  if (is_multi_event) {
	Rcpp::DataFrame df = Rcpp::DataFrame::create(Named("threshold") = res_thresh,
                                                 Named("scale") = res_size,
                                                 Named("count") = res_count);
    df["hit"] = res_me_a;
    df["fa"] = res_me_b;
    df["miss"] = res_me_c;
    df["cr"] = res_me_d;

	resultList["hira_me"] = df;
  }
  if (is_pragmatic) {

	Rcpp::DataFrame df = Rcpp::DataFrame::create(Named("threshold") = res_thresh,
                                                 Named("scale") = res_size,
                                                 Named("count") = res_count);
    df["bss"] = res_pra_bss;
    df["bs"] = res_pra_bs;
	resultList["hira_pragm"] = df;
  }

  if (is_csrr) {

	Rcpp::DataFrame df = Rcpp::DataFrame::create(Named("scale") = scales);
                                                 //,Named("count") = //TODO: FIX t

  NumericVector res_count(n_scales);
  NumericVector res_rps(n_scales);
  NumericVector res_csrr(n_scales);
    for (int sc = 0; sc < n_scales; sc++) {
        float rps = 0;
        float csrr = 0;
        res_count[sc] = no ;
		   float obs_with_rain = 0;
        for (int j = 0; j < no; j++) {
		      float acc_y = 0;
		      float acc_x = 0;

		      if (res_csrr_px(sc * no + j) > 0 ) { // th = 0 lowest threshold
			       obs_with_rain +=1;
			     }
	        for (int th = 0 ; th < n_thresholds ; th++) {
		    	int idx = th * n_scales * no + sc * no + j;
		    	acc_x += res_csrr_px(idx);
		    	acc_y += res_csrr_py(idx);
		    	rps += (acc_y-acc_x)*(acc_y-acc_x);
		    }

	  }
	  //TODO: Condition when values have zeros
	  Rcpp::Rcout << "n_thresholds" << n_thresholds <<"\n";
	  rps /= (n_thresholds - 1);

	  csrr = rps;
	  rps = no > 0? rps/no : -9999; //Rcpp::NumericVector::get_na();

	  csrr = obs_with_rain > 0 ? csrr/obs_with_rain: -9999; //Rcpp::NumericVector::get_na();
    csrr =  std::sqrt(csrr);
	  res_rps(sc) = rps;
	  res_csrr(sc) = csrr;

	}

	df["rps"] = res_rps;
	df["csrr"] = res_csrr;
	df["count"] = res_count;
	resultList["hira_csrr"] = df;

  }

  if (is_td) {
	Rcpp::DataFrame df = Rcpp::DataFrame::create(Named("threshold") = res_thresh,
                                                 Named("scale") = res_size,
                                                 Named("count") = res_count);

    df["hit"] = res_td_a;
    df["fa"] = res_td_b;
    df["miss"] = res_td_c;
    df["cr"] = res_td_d;

	resultList["hira_td"] = df;

  }

    //if (is_basic) {
	//  Rcpp::DataFrame df = Rcpp::DataFrame::create(Named("scale") = res_basic_size);
	//
    //  df["count"] = res_basic_count;
    //  df["bias"]  = res_basic_bias;
    //  df["mae"]   = res_basic_mae;
    //  df["mse"]   = res_basic_mse;
	//
	//  resultList["basic"] = df;
	//
    //}


  return resultList;
}
