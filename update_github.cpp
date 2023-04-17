#include <RcppArmadillo.h>
// [[Rcpp::depends(RcppArmadillo)]]
#include <RcppArmadilloExtensions/sample.h>
using namespace arma;

// [[Rcpp::export]]
double dnormLog2(arma::vec x, double means, double sds) {
  int n = x.size();
  double res=0;
  for(int i = 0; i < n; i++) {
    res += R::dnorm(x[i], means, sds, TRUE);
  }
  return res;
}

// [[Rcpp::export]]
double rgigRcpp(double lam, double chi, double psi){
  Rcpp::Environment package_env("package:GIGrvg");
  Rcpp::Function rfunction = package_env["rgig"];
  Rcpp::List test_out = rfunction(1, lam, chi, psi);
  return(test_out[0]);
}

// [[Rcpp::export]]
uvec seq_cpp(int locppid, int hicppid) {
  int n = hicppid - locppid + 1;
  uvec sequence(n);
  for(int i = 0; i < n; i++) {
    sequence[i] = locppid + i;
  }
  return sequence;
}

// [[Rcpp::export]]
arma::mat mvrnormArma(int n, arma::vec mu, arma::mat sigma) {
  int ncols = sigma.n_cols;
  arma::mat YY = arma::randn(n, ncols);
  return arma::repmat(mu, 1, n).t() + YY * arma::chol(sigma);
}

// [[Rcpp::export]]
double pos_phi(vec t_p, int wj, int wk, int Jsum, double a_phi, mat Lambda, mat zeta, 
               rowvec tau) {
  double res = R::dgamma(t_p(wj), a_phi, 1, 1) + log(t_p(wj));
  vec phi = t_p/ sum(t_p);
  arma::uvec idx0 = find(phi == 0);
  arma::uvec idx = find(phi != 0);
  double fi = min(phi.elem(idx));
  phi.elem(idx0).fill(fi);
  for(int jj = 0; jj < Jsum; jj++){
    double hssd = sqrt(zeta(jj, wk) *  phi(jj) * tau(wk) );
    res = res + R::dnorm(Lambda(jj, wk), 0, hssd, 1);
  }
  return(res);
}

// [[Rcpp::export]]
mat update_ystar(mat logY, mat logY1, int n, int Jsum, mat RSS, rowvec sig2, rowvec M){
  mat y_star(n, Jsum);
  for(int i = 0; i < n; i++) {
    for(int j = 0; j < Jsum; j++) {
      double u_min = R::pnorm(logY(i,j), RSS(i,j), sqrt(sig2(M(j))), 1, 0);
      double u_max = R::pnorm(logY1(i,j), RSS(i,j), sqrt(sig2(M(j))), 1, 0);
      double uij = R::runif(u_min, u_max);
      if(uij == 1) {uij = 1-9.9*pow(10, -17);}
      if(uij == 0) {uij = 9.9*pow(10, -17);}
      y_star(i,j) = R::qnorm(uij, RSS(i,j), sqrt(sig2(M(j))), 1, 0);
    }
  }
  return(y_star);
}

// [[Rcpp::export]]
rowvec update_sig2(int m, int n, mat RSS, rowvec J_bound,  rowvec J, double a_sig, double b_sig){
  rowvec res(m);
  for(int mi = 0; mi < m; mi++) {
    mat RSS_sub = RSS.cols(J_bound(mi) + 1*(mi!=0), J_bound(mi+1));
    res(mi) = 1.0/ (R::rgamma(a_sig + n * J(mi) * 0.5, 1.0/(b_sig+ 0.5 * sum(sum(RSS_sub % RSS_sub))) ) );
  }
  return(res);
}


// [[Rcpp::export]]
mat update_eta(int K, mat Lambda, mat Djminv, int n, mat RSS ){
  mat eta(n, K);
  mat Ik; Ik.eye(K, K);
  mat inv_e = (Ik + (Lambda.t() * Djminv * Lambda)).i();
  mat m_e = inv_e * Lambda.t() * Djminv;
  for(int i = 0; i < n; i++) {
    vec mu_e = m_e * (RSS.row(i)).t();
    eta.row(i) = mvrnormArma(1, mu_e,  inv_e);
  }
  return(eta);
}


// [[Rcpp::export]]
mat update_Lambda(int Jsum, int K, mat zeta, mat Z, rowvec tau, mat phi_m, mat eta, 
                  mat RSS, rowvec sig2, rowvec M ){
  mat res(Jsum, K);
  for(int j = 0; j < Jsum; j++) {
    rowvec Dj = zeta.row(j) % phi_m.row(j) % tau;
    Dj = 1.0/Dj;
    mat inv_lam = (diagmat(Dj) + (eta.t() * eta)/ (sig2(M(j))) ).i();
    vec mu_lam = inv_lam * eta.t() * RSS.col(j) / (sig2(M(j))) ;
    rowvec ttt = mvrnormArma(1, mu_lam,  inv_lam);
    arma::uvec idx0 = find(abs(ttt) < pow(10, -100));
    ttt.elem(idx0).fill(pow(10, -100));
    res.row(j) = ttt;
  }
  return(res);
}


// [[Rcpp::export]]
cube update_Z_Zeta(int Jsum, int K, mat LLambda, mat pphi_m, rowvec ttau, mat ZZ, mat zzeta){
  for(int j = 0; j < Jsum; j++) {
    for(int k = 0; k < K; k++) {
      zzeta(j, k) = 1.0/ (R::rgamma(1.0, 1.0/( 1.0/ZZ(j,k) + pow(LLambda(j,k), 2) /(2.0*pphi_m(j,k)*ttau(k))) ));
      ZZ(j, k) = 1.0/ (R::rgamma(1.0, 1.0/(1.0 + 1.0/zzeta(j, k)) ));
    }
  }
  cube res(Jsum, K, 2);
  res.slice(0) = zzeta;
  res.slice(1) = ZZ;
  return(res);
}

// [[Rcpp::export]]
cube update_phi_newad(int K, int Jsum, mat til_phi_m, mat sig_pro, mat loglambda_phi, mat mu_MH_phi,
                      double a_phi, mat Lambda, mat zeta, rowvec tau, mat acc,
                      mat phi_m, mat nacc, mat true_nacc, double acc_tar, 
                      double delta){
  for(int k = 0; k < K; k++) {
    for(int j = 0; j < Jsum; j++) {
      mat til_phi_pro = til_phi_m;
      double new_pro = R::rnorm(0, 1) * sqrt(exp(loglambda_phi(j,k)) *sig_pro(j, k)) + log(til_phi_m(j, k));
      til_phi_pro(j, k) = exp(new_pro);
      double d_log = pos_phi(til_phi_pro.col(k), j, k, Jsum, a_phi, Lambda, zeta, tau) -
        pos_phi(til_phi_m.col(k), j, k, Jsum, a_phi, Lambda, zeta, tau);
      vec tt(2); tt(0)=1; tt(1) = exp(d_log); acc(j, k) = min(tt);
      double cc = R::runif(0, 1);
      if(cc<acc(j, k)){
        til_phi_m(j, k) = exp(new_pro);
        vec ttt = til_phi_m.col(k) / sum(til_phi_m.col(k));
        arma::uvec idx = find(ttt < pow(10, -100));
        ttt.elem(idx).fill(pow(10, -100));
        phi_m.col(k) = ttt;
        nacc(j, k) ++;
        true_nacc(j, k) ++;
      }
      
      loglambda_phi(j,k) = loglambda_phi(j,k) + delta * (acc(j, k) - acc_tar);
      double mu_MH_phi_diff = log(til_phi_m(j, k)) - mu_MH_phi(j, k);
      mu_MH_phi(j, k) = mu_MH_phi(j, k) + delta * mu_MH_phi_diff;
      sig_pro(j, k) = sig_pro(j, k) + delta *(pow(mu_MH_phi_diff, 2) - sig_pro(j, k));
    }
  }
  
  cube res(Jsum, K, 8);
  res.slice(0) = til_phi_m;
  res.slice(1) = phi_m;
  res.slice(2) = nacc;
  res.slice(3) = true_nacc;
  res.slice(4) = sig_pro;
  res.slice(5) = loglambda_phi;
  res.slice(6) = mu_MH_phi;
  res.slice(7) = acc;
  return(res);
}

// [[Rcpp::export]]
cube update_phi(int K, int Jsum, mat til_phi_m, mat sig_pro,
                double a_phi, mat Lambda, mat zeta, rowvec tau, mat acc,
                mat phi_m, mat nacc, mat true_nacc, int ni, double acc_tar, 
                double delta){
  double ind;
  for(int k = 0; k < K; k++) {
    for(int j = 0; j < Jsum; j++) {
      mat til_phi_pro = til_phi_m;
      double new_pro = R::rnorm(0, 1) * sig_pro(j, k) + log(til_phi_m(j, k));
      til_phi_pro(j, k) = exp(new_pro);
      double d_log = pos_phi(til_phi_pro.col(k), j, k, Jsum, a_phi, Lambda, zeta, tau) -
        pos_phi(til_phi_m.col(k), j, k, Jsum, a_phi, Lambda, zeta, tau);
      vec tt(2); tt(0)=1; tt(1) = exp(d_log); acc(j, k) = min(tt);
      double cc = R::runif(0, 1);
      if(cc<acc(j, k)){
        til_phi_m(j, k) = exp(new_pro);
        vec ttt = til_phi_m.col(k) / sum(til_phi_m.col(k));
        arma::uvec idx = find(ttt < pow(10, -100));
        ttt.elem(idx).fill(pow(10, -100));
        phi_m.col(k) = ttt;
        nacc(j, k) ++;
        true_nacc(j, k) ++;
      }
      
      if(ni % 50 == 0){
        if(nacc(j, k)/50>acc_tar){
          ind = 1;
        } else{
          ind = -1;
        }
        sig_pro(j, k) = sig_pro(j, k) * exp(delta * ind);
        nacc(j, k) = 0;
      }
    }
  }
  
  cube res(Jsum, K, 5);
  res.slice(0) = til_phi_m;
  res.slice(1) = phi_m;
  res.slice(2) = nacc;
  res.slice(3) = true_nacc;
  res.slice(4) = sig_pro;
  return(res);
}

// [[Rcpp::export]]
rowvec update_tau_GIG(int K, double a_tau, double b_tau, int Jsum, mat Lambda, mat phi_m, mat zeta){
  rowvec rreess(K);
  for(int k = 0; k < K; k++) {
    double par1 = a_tau - Jsum/2.0;
    double par2 = sum( pow(Lambda.col(k), 2)  / (phi_m.col(k) % zeta.col(k)) );
    double par3 = 2.0 * b_tau;
    rreess(k) = rgigRcpp(par1, par2, par3);
  }
  return(rreess);
}

// [[Rcpp::export]]
cube update_psi_w_r(int m, int Lr, mat Si1, mat Si2, double a_w, double b_w, double a_psi_r){
  mat w_l_r_m(m, Lr);
  mat psi_r_m(m, Lr);
  
  for(int mi = 0; mi < m; mi++) {
    rowvec M_Lr(Lr);
    rowvec V_r(Lr);
    rowvec psi_r(Lr);
    rowvec w_l_r(Lr);
    for(int l=0; l< Lr; l++){
      M_Lr(l) = sum(Si1.row(mi)==l);
      w_l_r(l) = R::rbeta(sum( (Si1.row(mi)==l) % (Si2.row(mi) ==1)) + a_w,
            sum(  (Si1.row(mi)==l) % (Si2.row(mi) ==0)) + b_w);
    }
    for(int l=0; l< Lr; l++){
      uvec pos = seq_cpp(l+1, Lr-1);
      V_r(l) = R::rbeta(1+M_Lr(l), a_psi_r + sum(M_Lr.elem(pos)));
    }
    rowvec try1 = V_r; try1(Lr-1) = 1;
    rowvec try2(Lr);
    try2(0)=1;
    for(int l=1; l< Lr; l++){
      try2(l) = 1-V_r(l-1);
    }
    try2 = cumprod(try2);
    psi_r = try1 % try2;
    
    w_l_r_m.row(mi) = w_l_r;
    psi_r_m.row(mi) = psi_r;
  }
  
  cube res(m, Lr, 2);
  res.slice(0) = psi_r_m;
  res.slice(1) = w_l_r_m;
  return(res);
}

// [[Rcpp::export]]
cube update_Si12(int Lr, int n, int m,  mat ri, mat xi, double ur2, rowvec nu_r,
                 mat w_l_r_m, mat psi_r_m){
  vec piil1(Lr), piil0(Lr);
  mat Si1(m, n);
  mat Si2(m, n);
  for(int mi = 0; mi < m; mi++){
    for(int i = 0; i < n; i++) {
      for(int l = 0; l< Lr; l++){
        piil1(l)=R::dnorm(ri(mi, i), xi(mi, l), sqrt(ur2), 1) + log(w_l_r_m(mi, l)) + log(psi_r_m(mi, l));
        piil0(l)=R::dnorm(ri(mi, i),  (nu_r(mi)-w_l_r_m(mi, l)*xi(mi, l) )/(1-w_l_r_m(mi, l)) , sqrt(ur2), 1) + log(1-w_l_r_m(mi, l)) + log(psi_r_m(mi, l));
      }
      vec piil = join_cols(piil1,piil0);
      piil = exp(piil - max(piil));
      piil = piil/sum(piil);
      rowvec try4 = linspace<rowvec>(0, 2*Lr-1, 2*Lr);
      int id = Rcpp::RcppArmadillo::sample(try4, 1, false, piil)(0);
      
      if(id<Lr){
        Si1(mi,i) = id;
        Si2(mi,i) = 1;
      } else {
        Si1(mi,i) = id - Lr;
        Si2(mi,i) = 0;
      }
    }
  }
  cube res(m, n, 2);
  res.slice(0) = Si1;
  res.slice(1) = Si2;
  return(res);
}

// [[Rcpp::export]]
mat update_xi(int m, int Lr, mat ri, mat Si1, mat Si2, rowvec a_xi, rowvec sig2_xi_r,
              mat w_l_r_m, mat psi_r_m, rowvec nu_r, double ur2){
  mat xi(m, Lr);
  for(int mi = 0; mi < m; mi++){
    rowvec ri_sub = ri.row(mi);
    for(int l=0; l< Lr; l++){
      int sumSi1 = sum(Si1.row(mi)==l);
      int sumSi1Si20 = sum((Si1.row(mi)==l) % (Si2.row(mi) ==0));
      int sumSi1Si21 = sum((Si1.row(mi)==l) % (Si2.row(mi) ==1));
      
      if (sumSi1== 0){
        xi(mi, l) = R::rnorm(a_xi(mi), sqrt(sig2_xi_r(mi)));
      } else {
        double sci2 = sumSi1Si21 + sumSi1Si20 * pow(w_l_r_m(mi, l)/(1-w_l_r_m(mi, l)), 2);
        arma::uvec idx1 = find( (Si1.row(mi)==l)  % (Si2.row(mi) ==1) );
        arma::uvec idx0 = find( (Si1.row(mi)==l)  % (Si2.row(mi) ==0) );
        double tr = sum( ri_sub.elem(idx1) ) -w_l_r_m(mi, l)/(1-w_l_r_m(mi, l)) * sum( ri_sub.elem(idx0) - nu_r(mi)/(1-w_l_r_m(mi, l))  );
        double p_var = 1.0/(1.0/sig2_xi_r(mi)+sci2/ur2);
        double p_m = (a_xi(mi)/sig2_xi_r(mi) + tr/ur2) * p_var;
        xi(mi, l) = R::rnorm(p_m, sqrt(p_var));
      }
    }
  }
  return(xi);
}

// [[Rcpp::export]]
mat update_ri(int m, int n, double ur2, rowvec J, rowvec sig2, mat RSS, rowvec J_bound,
              mat Si1, mat Si2, rowvec nu_r, mat xi, mat w_l_r_m){
  mat ri(m, n);
  for(int mi = 0; mi < m; mi++) {
    double pos_r_var = 1.0/(1.0/ur2 + (J(mi))/(sig2(mi)));
    mat RSS_sub = RSS.cols(J_bound(mi) + 1*(mi!=0), J_bound(mi+1));
    for(int i = 0; i < n; i++) {
      double prior_r_m = Si2(mi, i)  *  xi(mi, Si1(mi, i)) + (1 - Si2(mi, i)) *
        (nu_r(mi)-w_l_r_m(mi, Si1(mi, i)) * xi(mi, Si1(mi, i)) )/(1-w_l_r_m(mi, Si1(mi, i)));
      double pos_r_m = pos_r_var * (prior_r_m/ur2+ sum(RSS_sub.row(i))/sig2(mi));
      ri(mi, i) =  R::rnorm(pos_r_m, sqrt(pos_r_var));
    }
  }
  return(ri);
}

// [[Rcpp::export]]
cube update_psi_w(int m, int Lalpha, rowvec J_bound, mat Sij1, mat Sij2,
                  double a_w_alpha, double b_w_alpha, double a_psi_alpha){
  mat w_l_a_m(m, Lalpha);
  mat psi_a_m(m, Lalpha);
  rowvec M_La(Lalpha);
  rowvec V_a(Lalpha);
  rowvec psi_a(Lalpha);
  rowvec w_l_a(Lalpha);
  for(int mi = 0; mi < m; mi++) {
    mat Sij1_sub = Sij1.cols(J_bound(mi)+ 1*(mi!=0), J_bound(mi+1));
    mat Sij2_sub = Sij2.cols(J_bound(mi)+ 1*(mi!=0), J_bound(mi+1));
    
    for(int l=0; l< Lalpha; l++){
      M_La(l) = sum(vectorise(Sij1_sub)==l);
      w_l_a(l) = R::rbeta( sum( (vectorise(Sij1_sub)==l) % (vectorise(Sij2_sub) ==1)) + a_w_alpha,
            sum( (vectorise(Sij1_sub)==l) % (vectorise(Sij2_sub) ==0)) + b_w_alpha);
    }
    w_l_a_m.row(mi) = w_l_a;
    
    for(int l=0; l< Lalpha; l++){
      uvec pos = seq_cpp(l+1, Lalpha-1);
      V_a(l) = R::rbeta(1+M_La(l), a_psi_alpha + sum(M_La.elem(pos)));
    }
    
    rowvec try1 = V_a; try1(Lalpha-1) = 1;
    rowvec try2(Lalpha);
    try2(0)=1;
    for(int l=1; l< Lalpha; l++){
      try2(l) = 1-V_a(l-1);
    }
    try2 = cumprod(try2);
    psi_a = try1 % try2;
    psi_a_m.row(mi) = psi_a;
  }
  
  cube res(m, Lalpha, 2);
  res.slice(0) = psi_a_m;
  res.slice(1) = w_l_a_m;
  return(res);
}

// [[Rcpp::export]]
cube update_Sij12(mat RSS, int s, int Jsum, 
                  int Lalpha, mat xi_alpha, rowvec nu_alpha, mat w_l_a_m, mat psi_a_m, rowvec M,
                  rowvec sig2, rowvec S){
  mat Sij1(s, Jsum);
  mat Sij2(s, Jsum);
  
  for(int i = 0; i < s; i++) {
    for(int j = 0; j < Jsum; j++) {
      vec pijl1(Lalpha), pijl0(Lalpha);
      
      vec RSS_subj = RSS.col(j);
      arma::uvec idx1 = find(S==i);
      for(int l = 0; l< Lalpha; l++){
        pijl1(l) = dnormLog2(RSS_subj.elem(idx1), xi_alpha(j, l), sqrt(sig2(M(j)))) + log(w_l_a_m(M(j), l)) + log(psi_a_m(M(j), l));
        pijl0(l) = dnormLog2(RSS_subj.elem(idx1),(nu_alpha(j) - xi_alpha(j, l) * w_l_a_m(M(j), l))/(1-w_l_a_m(M(j), l)), sqrt(sig2(M(j)))) + log(1-w_l_a_m(M(j), l)) + log(psi_a_m(M(j), l));
      }
      
      vec piil = join_cols(pijl1, pijl0);
      piil = exp(piil - max(piil));
      piil = piil/sum(piil);
      rowvec try4 = linspace<rowvec>(0, 2*Lalpha-1, 2*Lalpha);
      vec id = Rcpp::RcppArmadillo::sample(try4, 1, false, piil);
      
      if(id(0)<Lalpha){
        Sij1(i, j) = id(0);
        Sij2(i, j) = 1;
      } else {
        Sij1(i, j) = id(0) - Lalpha;
        Sij2(i, j) = 0;
      }
    }
  }
  
  cube res(s, Jsum, 2);
  res.slice(0) = Sij1;
  res.slice(1) = Sij2;
  return(res);
}


// [[Rcpp::export]]
mat update_xi_alpha(int Lalpha, int Jsum, mat RSS, mat IIJ1, mat IIJ2,
                    rowvec u2_alpha, rowvec a_xi_alpha, rowvec M, 
                    mat w_l_a_m, rowvec sig2, rowvec nu_alpha){
  mat res(Jsum, Lalpha);
  for(int l=0; l< Lalpha; l++){
    for(int j = 0; j < Jsum; j++) {
      vec RSS_subj = RSS.col(j);
      int sumIIJ1 = sum(IIJ1.col(j)==l);
      double sumIIJ1IIJ20 = sum((IIJ1.col(j)==l) % (IIJ2.col(j) ==0));
      double sumIIJ1IIJ21 = sum((IIJ1.col(j)==l) % (IIJ2.col(j) ==1));
      
      if (sumIIJ1== 0){
        res(j, l) = R::rnorm(0, 1) * sqrt(u2_alpha(j)) + a_xi_alpha(j);
      } else {
        double sci2 = sumIIJ1IIJ21 + sumIIJ1IIJ20 * pow(w_l_a_m(M(j), l)/(1-w_l_a_m(M(j), l)), 2);
        arma::uvec idx1 = find( (IIJ1.col(j)==l)  % (IIJ2.col(j)==1) );
        arma::uvec idx0 = find( (IIJ1.col(j)==l)  % (IIJ2.col(j)==0) );
        double p_var = 1.0/(1.0/u2_alpha(j)+sci2/sig2(M(j)));
        double tr = sum( RSS_subj.elem(idx1) ) - w_l_a_m(M(j), l)/(1-w_l_a_m(M(j), l)) * sum( RSS_subj.elem(idx0) - nu_alpha(j)/(1-w_l_a_m(M(j), l)) );
        double p_m = (a_xi_alpha(j)/u2_alpha(j) + tr/sig2(M(j))) * p_var;
        res(j, l) = R::rnorm(p_m, sqrt(p_var));
      }
    }
  }
  return(res);
}


