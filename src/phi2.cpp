/* Determine latitude angle phi-2. */

#include <math.h>
#include <limits>

#include "proj.h"
#include "proj_internal.h"

static const double TOL = 1.0e-10;
static const int N_ITER = 15;

double pj_sinhpsi2tanphi(projCtx ctx, const double taup, const double e) {
  /****************************************************************************
  * Convert tau' = sinh(psi) = tan(chi) to tau = tan(phi).  The code is taken
  * from GeographicLib::Math::tauf(taup, e).
  *
  * Here
  *   phi = geographic latitude (radians)
  * psi is the isometric latitude
  *   psi = asinh(tan(phi)) - e * atanh(e * sin(phi))
  *       = asinh(tan(chi))
  * chi is the conformal latitude
  *
  * The representation of latitudes via their tangents, tan(phi) and tan(chi),
  * maintains full *relative* accuracy close to latitude = 0 and +/- pi/2.
  * This is sometimes important, e.g., to compute the scale of the transverse
  * Mercator projection which involves cos(phi)/cos(chi) tan(phi)
  *
  * From Karney (2011), Eq. 7,
  *
  *   tau' = sinh(psi) = sinh(asinh(tan(phi)) - e * atanh(e * sin(phi)))
  *        = tan(phi) * cosh(e * atanh(e * sin(phi))) -
  *          sec(phi) * sinh(e * atanh(e * sin(phi)))
  *        = tau * sqrt(1 + sigma^2) - sqrt(1 + tau^2) * sigma
  * where
  *   sigma = sinh(e * atanh( e * tau / sqrt(1 + tau^2) ))
  *
  * For e small,
  *
  *    tau' = (1 - e^2) * tau
  *
  * The relation tau'(tau) can therefore by reliably inverted by Newton's
  * method with
  *
  *    tau = tau' / (1 - e^2)
  *
  * as an initial guess.  Newton's method requires dtau'/dtau.  Noting that
  *
  *   dsigma/dtau = e^2 * sqrt(1 + sigma^2) /
  *                 (sqrt(1 + tau^2) * (1 + (1 - e^2) * tau^2))
  *   d(sqrt(1 + tau^2))/dtau = tau / sqrt(1 + tau^2)
  *
  * we have
  *
  *   dtau'/dtau = (1 - e^2) * sqrt(1 + tau'^2) * sqrt(1 + tau^2) /
  *                (1 + (1 - e^2) * tau^2)
  *
  * This works fine unless tau^2 and tau'^2 overflows.  This may be partially
  * cured by writing, e.g., sqrt(1 + tau^2) as hypot(1, tau).  However, nan
  * will still be generated with tau' = inf, since (inf - inf) will appear in
  * the Newton iteration.
  *
  * If we note that for sufficiently large |tau|, i.e., |tau| >= 2/sqrt(eps),
  * sqrt(1 + tau^2) = |tau| and
  *
  *   tau' = exp(- e * atanh(e)) * tau
  *
  * So
  *
  *   tau = exp(e * atanh(e)) * tau'
  *
  * can be returned unless |tau| >= 2/sqrt(eps); this then avoids overflow
  * problems for large tau' and returns the correct result for tau' = +/-inf
  * and nan.
  *
  * Newton's method usually take 2 iterations to converge to double precision
  * accuracy (for WGS84 flattening).  However only 1 iteration is needed for
  * |chi| < 3.35 deg.  In addition, only 1 iteration is needed for |chi| >
  * 89.18 deg (tau' > 70), if tau = exp(e * atanh(e)) * tau' is used as the
  * starting guess.
  ****************************************************************************/

  constexpr int numit = 5;
  // min iterations = 1, max iterations = 2; mean = 1.954
  constexpr double tol = sqrt(std::numeric_limits<double>::epsilon()) / 10;
  constexpr double tmax = 2 / sqrt(std::numeric_limits<double>::epsilon());
  double
    e2m = 1 - e * e,
    tau = fabs(taup) > 70 ? taup * exp(e * atanh(e)) : taup / e2m,
    stol = tol * std::max(1.0, fabs(taup));
  if (!(fabs(tau) < tmax)) return tau; // handles +/-inf and nan and e = 1
  // if (e2m < 0) return std::numeric_limits<double>::quiet_NaN();
  int i = numit;
  for (; i; --i) {
    double tau1 = sqrt(1 + tau * tau),
      sig = sinh( e * atanh(e * tau / tau1) ),
      taupa = sqrt(1 + sig * sig) * tau - sig * tau1,
      dtau = (taup - taupa) * (1 + e2m * tau * tau) /
      ( e2m * sqrt(1 + tau * tau) * sqrt(1 + taupa * taupa) );
    tau += dtau;
    if (!(fabs(dtau) >= stol))
      break;
  }
  if (i == 0)
    pj_ctx_set_errno(ctx, PJD_ERR_NON_CONV_SINHPSI2TANPHI);
  return tau;
}

/*****************************************************************************/
double pj_phi2(projCtx ctx, const double ts0, const double e) {
  /****************************************************************************
   * Determine latitude angle phi-2.
   * Inputs:
   *   ts = exp(-psi) where psi is the isometric latitude (dimensionless)
   *   e = eccentricity of the ellipsoid (dimensionless)
   * Output:
   *   phi = geographic latitude (radians)
   * Here isometric latitude is defined by
   *   psi = log( tan(pi/4 + phi/2) *
   *              ( (1 - e*sin(phi)) / (1 + e*sin(phi)) )^(e/2) )
   *       = asinh(tan(phi)) - e * atanh(e * sin(phi))
   *
   * OLD: This routine inverts this relation using the iterative scheme given
   * by Snyder (1987), Eqs. (7-9) - (7-11).
   *
   * NEW: This routine writes converts t = exp(-psi) to
   *
   *   tau' = sinh(psi) = (1/t - t)/2
   *
   * returns atan(sinpsi2tanphi(tau'))
   ***************************************************************************/
  return atan(pj_sinhpsi2tanphi(ctx, (1/ts0 - ts0) / 2, e));
}
