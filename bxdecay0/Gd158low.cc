// Copyright 1995-2016 V.I. Tretyak
// Copyright 2011-2017 F. Mauger
//
// This program is free software: you  can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free  Software Foundation, either  version 3 of the  License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

// Ourselves:
#include <bxdecay0/Gd158low.h>

// Standard library:
#include <sstream>
#include <stdexcept>
#include <cmath>

// This project:
#include <bxdecay0/i_random.h>
#include <bxdecay0/event.h>
#include <bxdecay0/alpha.h>
#include <bxdecay0/gamma.h>
#include <bxdecay0/electron.h>
#include <bxdecay0/positron.h>
#include <bxdecay0/particle.h>
#include <bxdecay0/pair.h>
#include <bxdecay0/nucltransK.h>
#include <bxdecay0/nucltransKL.h>
#include <bxdecay0/nucltransKLM.h>
#include <bxdecay0/nucltransKLM_Pb.h>
#include <bxdecay0/beta.h>
#include <bxdecay0/beta1.h>
#include <bxdecay0/beta2.h>
#include <bxdecay0/beta_1fu.h>
#include <bxdecay0/PbAtShell.h>

namespace bxdecay0 {

  void Gd158low(i_random & prng_,
                event & event_,
                const int levelkev_)
  {
    //double t;
    double tdlev;
    double tclev;
    double thlev;
    // Subroutine describes the deexcitation process in Gd158 nucleus
    // after 2b-decay of Dy158 to ground and excited 0+ and 2+ levels
    // of Gd158 (NNDC site on 10.10.2010, NDS 101(2004)325).
    // Call : call Gd158low(levelkev_)
    // Input : levelkev_ - energy of Gd158 level (integer in keV) occupied
    // initially
    // 0+(gs) - 0 keV,
    // 2+(1) - 80 keV,
    // 4+(1) - 261 keV.
    // Output: // common/genevent/tevst,npfull,npgeant(100),pmoment(3,100),// ptime(100).
    // VIT, 10.10.2010.
    tclev=0.;
    if (levelkev_ == 261) goto label_261;
    if (levelkev_ ==  80) goto label_80;
    if (levelkev_ ==  0) goto label_10000;
    goto label_20000;
  label_261  :
    thlev=0.148e-9;
    decay0_nucltransK(prng_, event_, 0.182,0.050,0.308,0.,tclev,thlev,tdlev);
    goto label_80;
  label_80  :
    thlev=2.52e-9;
    decay0_nucltransK(prng_, event_, 0.080,0.050,6.02,0.,tclev,thlev,tdlev);
    return;
  label_10000  :
    return;
  label_20000  :
    // print *,'Gd158: wrong level [keV] ',levelkev_
    return;
  }
  // end of Gd158low.f




} // end of namespace bxdecay0

// end of Gd158low.cc
// Local Variables: --
// mode: c++ --
// End: --
