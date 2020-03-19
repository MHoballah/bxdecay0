// Copyright 2020 F. Mauger
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
// decay0_generator.cc

// Ourselves:
#include <bxdecay0/dbd_gA.h>

// Standard library:
#include <limits>
#include <stdexcept>
#include <set>
#include <sstream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <random>
#include <iomanip>
#include <math.h>
// This project:
#include <bxdecay0/resource.h>
#include <bxdecay0/event.h>
// Third party:
#include <gsl/gsl_errno.h>
#include <gsl/gsl_interp2d.h>

namespace {

  /// \brief Tabulated kinetic energy model for both emitted electrons:
  struct tabulated_pdf_type {
    
    enum sampling_type {
      SAMPLING_UNDEF     = 0, ///< Undefined sampling mode
      SAMPLING_LINEAR    = 1, ///< Linear sampling mode
      SAMPLING_NONLINEAR = 2,  ///< Irregular sampling mode
      SAMPLING_CUMULATIVE = 3, ///< Cumulative probability
    };
    
    tabulated_pdf_type() = default;

    ~tabulated_pdf_type() {      
      return;
    }
    
    // Attributes:
    double              esum_max = -1.0; ///< Maximum sum of kinetic energy, a.k.a. Qbb (unit: MeV)
    sampling_type       e_sampling[2] = {SAMPLING_UNDEF,SAMPLING_UNDEF}; ///< Sampling mode
    unsigned int        e_nsamples[2] = {0, 0};  ///< Number of kinetic energy samples
    double              e_min[2] = {-1.0, -1.0}; ///< Minimum kinetic energy (unit: MeV)
    double              e_max[2] = {-2.0, -2.0}; ///< Maximum kinetic energy (unit: MeV)
    std::vector<double> e_samples[2];            ///< Kinetic energy samples (unit: MeV)
    std::vector<double> prob;   ///< p.d.f. 2D-sampled values (not necessarily normalized)
    double prob_max     = -1.0; ///< Maxi value for the tabulated p.d.f. (unit: arbitrary)
    /// Malak's Work
    std::vector<std::vector<double>> save_probability; ///< Save probability from Ga files
    std::vector<std::vector<double>> bound;
    std::vector<double> energies;
    double energy_step;
    unsigned int prob_samples;
    double proba, beta=0, sum_proba =0;;
    unsigned int iteration = 0;

  };

  /// \brief p.d.f. interpolator
  struct pdf_interpolator_type {
    
    pdf_interpolator_type() = default;

    ~pdf_interpolator_type() {
      if (xacc) {
        gsl_interp_accel_free(xacc);
      }
      if (yacc) {
        gsl_interp_accel_free(yacc);
      }
      if (interp) {
        gsl_interp2d_free(interp);
      }
      return;
    }
    
    // Attributes:
    const gsl_interp2d_type * choice = gsl_interp2d_bilinear; ///< GSL interpolation type
    gsl_interp2d     * interp = nullptr; ///< GSL interpolator
    gsl_interp_accel * xacc = nullptr;   ///< GSL interpolation accelerator for E1 sampling
    gsl_interp_accel * yacc = nullptr;   ///< GSL interpolation accelerator for E2 sampling
    
  };
  
}

namespace bxdecay0 {

  /// \brief PIMPL type
  struct dbd_gA::pimpl_type {
    pimpl_type() = default;
    ~pimpl_type(){}
    // Attributes:
    ::tabulated_pdf_type    tab_pdf;          ///< Tabulated joint p.d.f.
    ::pdf_interpolator_type pdf_interpolator; ///< 2D-interpolator
  };
  
  void dbd_gA::pimpl_deleter_type::operator()(dbd_gA::pimpl_type * ptr_) const
  {
    delete ptr_;
    return;
  }
  
  // static
  bool dbd_gA::is_nuclide_supported(const std::string & id_)
  {
    static const std::set<std::string> _sn({"Se82", "Mo100", "Cd116", "Nd150"}); 
    return _sn.count(id_);
  }

  // static
  std::string dbd_gA::process_label(const process_type p_)
  {
    switch (p_) {
    case PROCESS_G0 : return "g0";
    case PROCESS_G2 : return "g2";
    case PROCESS_G22 : return "g22";
    case PROCESS_G4 : return "g4";
    }
    return "";
  }

  dbd_gA::~dbd_gA()
  {
    if (is_initialized()) {
      reset();
    }
    return;
  }
  void dbd_gA::set_nuclide(const std::string & nuclide_)
  {
    if (!is_nuclide_supported(nuclide_) and nuclide_ != "Test") {
      throw std::logic_error("bxdecay0::dbd_gA::set_nuclide: Unsupported nuclide '"
                             + nuclide_ + "'!");
    }
    _nuclide_ = nuclide_;
    return;
  }

  void dbd_gA::set_process(const process_type process_)
  {
    if (process_ == PROCESS_UNDEF) {
      throw std::logic_error("bxdecay0::dbd_gA::set_process: Invalid gA process !");
    }
    _process_ = process_;
    return; 
  }

  void dbd_gA::set_shooting(const shooting_type shooting_)
  {
    if (shooting_ == SHOOTING_UNDEF) {
      throw std::logic_error("bxdecay0::dbd_gA::set_shooting: Invalid shooting method !");
    }
    _shooting_ = shooting_;
    return; 
  }

  /*void dbd_gA::set_number_simulated(const unsigned int number_){
    _number_ = number_;
    return;
    }*/
  
  /*bool dbd_gA::verify_CP_simulation (std::bool & verify_){
    if(verify_ == true)
      {
	_verify_ = verify_;
      }
    return _verify_;
    }*/

  bool dbd_gA::is_initialized() const
  {
    return _initialized_;
  }

  void dbd_gA::initialize()
  {
    if (is_initialized()) {
      throw std::logic_error("bxdecay0::dbd_gA::initialize: Generator is already initialized!");
    }
    if (_nuclide_.empty()) {
      throw std::logic_error("bxdecay0::dbd_gA::initialize: Missing nuclide!");
    }
    if (_process_ == PROCESS_UNDEF) {
      throw std::logic_error("bxdecay0::dbd_gA::initialize: Missing process!");
    }
    if (_shooting_ == SHOOTING_UNDEF) {
      throw std::logic_error("bxdecay0::dbd_gA::initialize: Missing shooting method!");
      // Default to rejection sampling method:
      _shooting_ = SHOOTING_REJECTION;
    }
    /*if(_number_ == 0) {
	throw std::logic_error("bxdecay0::dbd_gA::set_number_simulated: Invalid number of simulations!");      
	}*/
   
    // Build the path of the tabulated p.d.f. resource file:
    {
      std::ostringstream outs;
      outs << "data/dbd_gA/" << _nuclide_ << '/' << process_label(_process_) << '/'
           << "tab_pdf.data";
      _tabulated_pdf_file_path_ = outs.str();
      std::string fres = get_resource(_tabulated_pdf_file_path_, true);
      _tabulated_pdf_file_path_ = fres;
    }
    
    // _pimpl_ = std::make_unique<pimpl_type>();
    _pimpl_.reset(new pimpl_type);
    
    if (_shooting_ == SHOOTING_REJECTION) {
    _load_tabulated_pdf_();
    _build_pdf_interpolator_();
    }
    if (_shooting_ == SHOOTING_INVERSE_TRANSFORM_METHOD) {
      _load_tabulated_probability_();
    }
    
    _initialized_ = true;
    return;
  }

  void dbd_gA::_build_pdf_interpolator_()
  {
    auto & pdf_interpolator = _pimpl_->pdf_interpolator;
    unsigned int nx = _pimpl_->tab_pdf.e_samples[0].size();
    unsigned int ny = _pimpl_->tab_pdf.e_samples[1].size();
    pdf_interpolator.interp = gsl_interp2d_alloc(pdf_interpolator.choice, nx, ny);
    /* initialize interpolation */
    std::cout << "problem interpolator " << std::endl;
    gsl_interp2d_init(pdf_interpolator.interp,
                      _pimpl_->tab_pdf.e_samples[0].data(),
                      _pimpl_->tab_pdf.e_samples[1].data(),
                      _pimpl_->tab_pdf.prob.data(),
                      nx,
                      ny);
    return;
  }

  void dbd_gA::plot_interpolated_pdf(std::ostream & out_, const unsigned int nsamples_) const
  {
    double e1_min = _pimpl_->tab_pdf.e_samples[0].front();
    double e1_max = _pimpl_->tab_pdf.e_samples[0].back();
    double e1_step = (e1_max - e1_min) / nsamples_;
    double e2_min = _pimpl_->tab_pdf.e_samples[1].front();
    double e2_max = _pimpl_->tab_pdf.e_samples[1].back();
    double e2_step = (e2_max - e2_min) / nsamples_;
    for (unsigned int i = 0; i < nsamples_; ++i) {
      double xi = e1_min + (i + 0.5) * e1_step;
      for (unsigned int j = 0; j < nsamples_; ++j) {
        double yj = e2_min + (j + 0.5) * e2_step;
        double prob;
        int err_code = gsl_interp2d_eval_e(_pimpl_->pdf_interpolator.interp,
                                           _pimpl_->tab_pdf.e_samples[0].data(),
                                           _pimpl_->tab_pdf.e_samples[1].data(),
                                           _pimpl_->tab_pdf.prob.data(),
                                           xi,
                                           yj,
                                           _pimpl_->pdf_interpolator.xacc,
                                           _pimpl_->pdf_interpolator.yacc,
                                           &prob);
        if (err_code == GSL_EDOM) {
          throw std::logic_error("bxdecay0::dbd_gA::plot_interpolated_pdf: Invalid E1/E2 values for interpolation!");
        }
        out_ <<  xi << ' ' <<  yj << ' ' << prob << std::endl;
      }
      out_ << std::endl;
    }
    
    return;
  }
 
  void dbd_gA::_load_tabulated_pdf_()
  {
    std::ifstream f_tab_pdf(_tabulated_pdf_file_path_.c_str());
    if (! f_tab_pdf) {
      throw std::logic_error("bxdecay0::dbd_gA::_load_tabulated_pdf_: Cannot open file '"
                             + _tabulated_pdf_file_path_ + "'!");
    }
    bool debug = false;
    debug = true;
    unsigned int nlines = 0;
    int beta_index = 0; // Next expected beta (0 or 1)
    int prob_index = 0; // Next expected prob sample
    double esum_max = -1.0;
    while (f_tab_pdf) {
      std::string raw_line;
      std::getline(f_tab_pdf, raw_line);
      nlines++;
      if (debug) {
        std::cerr << "[debug] bxdecay0::dbd_gA::_load_tabulated_pdf_: raw_line = <" << raw_line << '>' << std::endl;
      }
      if (raw_line.size() == 0) {
        if (debug) {
          std::cerr << "[debug] bxdecay0::dbd_gA::_load_tabulated_pdf_: Skip blank line." << std::endl;
        }
        continue;
      }
      {
        std::string first_word;
        std::istringstream ins(raw_line);
        ins >> first_word;
        if (first_word[0] == '#') {
          if (debug) {
            std::cerr << "[debug] bxdecay0::dbd_gA::_load_tabulated_pdf_: Skip comment line." << std::endl;
          }
          continue;
        }
      }
      if (_pimpl_->tab_pdf.esum_max < 0.0) {
        std::istringstream line_in(raw_line);
        line_in >> _pimpl_->tab_pdf.esum_max;
        if (!line_in) {
          throw std::logic_error("bxdecay0::dbd_gA::_load_tabulated_pdf_: Format error at line #"
                                 + std::to_string(nlines) + "!");
        }
        if (_pimpl_->tab_pdf.esum_max <= 0.0) {
          throw std::logic_error("bxdecay0::dbd_gA::_load_tabulated_pdf_: Invalid Esum max value!");
        }
     } else if (beta_index < 2) {
        // Parse E1 and E2 sampling first:
        // if (_pimpl_->tab_pdf.e_sampling[beta_index] != SAMPLING_UNDEF) {
        //   beta_index++;
        // }
        std::istringstream line_in(raw_line);
        // Parse beta energy sampling:
        if (debug) {
          std::cerr << "[debug] bxdecay0::dbd_gA::_load_tabulated_pdf_: Parsing Beta #"
                    << (beta_index + 1) << " kinetic energy sampling..." << std::endl;
        }
        std::string sampling_label;
        line_in >> sampling_label;
        if (sampling_label == "linear") {
          _pimpl_->tab_pdf.e_sampling[beta_index] = tabulated_pdf_type::SAMPLING_LINEAR;
          line_in >> _pimpl_->tab_pdf.e_nsamples[beta_index]
                  >> _pimpl_->tab_pdf.e_min[beta_index]
                  >> _pimpl_->tab_pdf.e_max[beta_index];
          if (!line_in) {
            throw std::logic_error("bxdecay0::dbd_gA::_load_tabulated_pdf_: Format error at line #"
                                   + std::to_string(nlines) + "!");
          }
          if (_pimpl_->tab_pdf.e_nsamples[beta_index] < 3) {
            throw std::logic_error("bxdecay0::dbd_gA::_load_tabulated_pdf_: Invalid E number of samples!");
          }
          if (_pimpl_->tab_pdf.e_min[beta_index] < 0.0 or
              _pimpl_->tab_pdf.e_min[beta_index] >= _pimpl_->tab_pdf.e_max[beta_index]) {
            throw std::logic_error("bxdecay0::dbd_gA::_load_tabulated_pdf_: Invalid E range!");
          }
          _pimpl_->tab_pdf.e_samples[beta_index].reserve(_pimpl_->tab_pdf.e_nsamples[beta_index]);
          double de = (_pimpl_->tab_pdf.e_max[beta_index] - _pimpl_->tab_pdf.e_min[beta_index]) / (_pimpl_->tab_pdf.e_nsamples[beta_index] - 1);
          for (unsigned int i = 0; i < _pimpl_->tab_pdf.e_nsamples[beta_index]; i++) {
            double ei =  _pimpl_->tab_pdf.e_min[beta_index] + i * de;
            _pimpl_->tab_pdf.e_samples[beta_index].push_back(ei);
          }
          if (debug) {
            std::cerr << "[debug] bxdecay0::dbd_gA::_load_tabulated_pdf_: Number of energy sampling points = "
                      << std::to_string(_pimpl_->tab_pdf.e_nsamples[beta_index]) << std::endl;
            std::cerr << "[debug] bxdecay0::dbd_gA::_load_tabulated_pdf_: Energy sampling step = "
                      << std::to_string(de) << " MeV" << std::endl;
            std::cerr << "[debug] bxdecay0::dbd_gA::_load_tabulated_pdf_: Minimum sampled energy = "
                      << std::to_string(_pimpl_->tab_pdf.e_samples[beta_index].front()) << " MeV" << std::endl;
            std::cerr << "[debug] bxdecay0::dbd_gA::_load_tabulated_pdf_: Maximum sampled energy = "
                      << std::to_string(_pimpl_->tab_pdf.e_samples[beta_index].back()) << " MeV" << std::endl;
            std::cerr << "[debug] bxdecay0::dbd_gA::_load_tabulated_pdf_: E sampling is parsed." << std::endl;
          }
        } else if (sampling_label == "nonlinear") {
          throw std::logic_error("bxdecay0::dbd_gA::_load_tabulated_pdf_: Not supported E samplig mode!");          
          // _pimpl_->tab_pdf.e_sampling[beta_index] = tabulated_pdf_type::SAMPLING_NONLINEAR;
          // line_in >> _pimpl_->tab_pdf.e_nsamples[beta_index] >> std::ws;
          // if (_pimpl_->tab_pdf.e_nsamples[beta_index] < 3) {
          //   throw std::logic_error("bxdecay0::dbd_gA::_load_tabulated_pdf_: Invalid E number of samples!");
          // }
          // _pimpl_->tab_pdf.e_samples[beta_index].reserve(_pimpl_->tab_pdf.e_nsamples[beta_index]);
          // while (line_in and ! line_in.eof()) {
          //   double e;
          //   line_in >> e;
          //   if (_pimpl_->tab_pdf.e_samples[beta_index].size()) {
          //     if (e <  _pimpl_->tab_pdf.e_samples[beta_index].back()) {
          //       throw std::logic_error("bxdecay0::dbd_gA::_load_tabulated_pdf_: Invalid E sample value (lower than the last sampled value)!");
          //     }
          //   }
          //   _pimpl_->tab_pdf.e_samples[beta_index].push_back(e);
          // }
        } else {
          throw std::logic_error("bxdecay0::dbd_gA::_load_tabulated_pdf_: Invalid E sampling mode!");
        }
        beta_index++;
      } else if (prob_index < (_pimpl_->tab_pdf.e_nsamples[0] * _pimpl_->tab_pdf.e_nsamples[1])) {
        unsigned int n1 = _pimpl_->tab_pdf.e_nsamples[0];
        unsigned int n2 = _pimpl_->tab_pdf.e_nsamples[1];
        _pimpl_->tab_pdf.prob.reserve(n1 * n2);
        std::istringstream line_in(raw_line);
        while (line_in and !line_in.eof()) {
          std::string word;
          line_in >> word;
          if (word.size()) {
            if (word[0] == '#') {
              continue;
            } else {
              std::istringstream word_in(word);
              double prob;
              word_in >> prob;
              if (!word_in) {
                throw std::logic_error("bxdecay0::dbd_gA::_load_tabulated_pdf_: Format error for prob at line #"
                                       + std::to_string(nlines) + "!");     
              }
              if (prob < 0.0) {
                throw std::logic_error("bxdecay0::dbd_gA::_load_tabulated_pdf_: Invalid p.d.f. value [" + std::to_string(prob) + "] at line #"
                                       + std::to_string(nlines) + "!");
              }
              int index1 = prob_index / n2;
              int index2 = prob_index % n2;
              double e1 = _pimpl_->tab_pdf.e_samples[0][index1];
              double e2 = _pimpl_->tab_pdf.e_samples[1][index2];
              if (e1 + e2 > _pimpl_->tab_pdf.esum_max) {
                if (prob > 0.0) {
                  throw std::logic_error("bxdecay0::dbd_gA::_load_tabulated_pdf_: Invalid p.d.f. value [" + std::to_string(prob) + "] at line #"
                                         + std::to_string(nlines) + "! Should be zero!");
                }
              }
              _pimpl_->tab_pdf.prob.push_back(prob);
              prob_index++; // skip to next expected sample
              if (_pimpl_->tab_pdf.prob_max < 0.0 or prob > _pimpl_->tab_pdf.prob_max) {
                _pimpl_->tab_pdf.prob_max = prob;
              }
              if (prob_index == n1 * n2) {
                if (debug) {
                  std::cerr << "[debug] bxdecay0::dbd_gA::_load_tabulated_pdf_: Number of samples have been reached." << std::endl;
                }
                break;
              }
            }
            line_in >> std::ws;
          } 
        }
        if (prob_index == n1 * n2) {
          if (debug) {
            std::cerr << "[debug] bxdecay0::dbd_gA::_load_tabulated_pdf_: Add sampling data are loaded." << std::endl;
          }
          break;
        }
      }
      if (f_tab_pdf.eof()) {
        if (debug) {
          std::cerr << "[debug] bxdecay0::dbd_gA::_load_tabulated_pdf_: End of parsing input." << std::endl;
        }
        break;
      }
    }
    return;
  }

  void dbd_gA::_load_tabulated_probability_()
  {

    std::ifstream f_tab_pdf(_tabulated_pdf_file_path_.c_str());
    if (! f_tab_pdf) {
      throw std::logic_error("bxdecay0::dbd_gA::_load_tabulated_probability_: Cannot open file '"
                             + _tabulated_pdf_file_path_ + "'!");
    }
    bool debug = false;
    debug = true;
    unsigned int nlines = 0;
    
    while (f_tab_pdf) {
      std::string raw_line;
      std::getline(f_tab_pdf, raw_line);
      nlines++;
      if (debug) {
        std::cerr << "[debug] bxdecay0::dbd_gA::_load_tabulated_probability_: raw_line = <" << raw_line << '>' << std::endl;
	}
      if (raw_line.size() == 0) {
        if (debug) {
          std::cerr << "[debug] bxdecay0::dbd_gA::_load_tabulated_probability_: Skip blank line." << std::endl;
        }
        continue;
      }
      {
        std::string first_word;
        std::istringstream ins(raw_line);
        ins >> first_word;
        if (first_word[0] == '#') {
          if (debug) {
            std::cerr << "[debug] bxdecay0::dbd_gA::_load_tabulated_probability_: Skip comment line." << std::endl;
          }
          continue;
        }
      }
      
      if (_pimpl_->tab_pdf.esum_max < 0.0) {
	std::istringstream line_in(raw_line);
        line_in >> _pimpl_->tab_pdf.esum_max;
	std::cout << "esum = " << _pimpl_->tab_pdf.esum_max << std::endl;
        if (!line_in) {
          throw std::logic_error("bxdecay0::dbd_gA::_load_tabulated_probability_: Format error at line #"
                                 + std::to_string(nlines) + "!");
        }
        if (_pimpl_->tab_pdf.esum_max <= 0.0) {
          throw std::logic_error("bxdecay0::dbd_gA::_load_tabulated_probability_: Invalid Esum max value!");
        }
	continue;
      }
      std::istringstream line_in(raw_line);
      std::string sampling_label;
      line_in >> sampling_label;
      std::cout << "sampling mode " << sampling_label << std::endl;

      if (sampling_label == "CumulativeProbability"){

	
	  _pimpl_->tab_pdf.e_sampling[0] = tabulated_pdf_type::SAMPLING_CUMULATIVE;
	  _pimpl_->tab_pdf.e_sampling[1] = tabulated_pdf_type::SAMPLING_CUMULATIVE;

	  line_in >> _pimpl_->tab_pdf.e_min[0]
		  >> _pimpl_->tab_pdf.e_max[0]
		  >> _pimpl_->tab_pdf.energy_step
		  >> _pimpl_->tab_pdf.prob_samples;

	_pimpl_->tab_pdf.e_min[1]=_pimpl_->tab_pdf.e_min[0];
	_pimpl_->tab_pdf.e_max[1]=_pimpl_->tab_pdf.e_max[0];

	if (!line_in) {
	  throw std::logic_error("bxdecay0::dbd_gA::_load_tabulated_probability_: Format error at line #"
				 + std::to_string(nlines) + "!");
	}

	if (_pimpl_->tab_pdf.e_min[0] < 0.0 or
	    _pimpl_->tab_pdf.e_min[0] >= _pimpl_->tab_pdf.e_max[0]) {
	  throw std::logic_error("bxdecay0::dbd_gA::_load_tabulated_probability_: Invalid E range!");
	}
	std::cout <<  _pimpl_->tab_pdf.prob_samples << std::endl;
	
	_pimpl_->tab_pdf.save_probability.reserve(_pimpl_->tab_pdf.prob_samples);
	//_pimpl_->tab_pdf.save_probability[].reserve(3000);

	//// save stored probabilities
	std::string line;
	int count =0;
	while(f_tab_pdf){
	std::getline(f_tab_pdf, raw_line);
        std::istringstream line_ins(raw_line);
	{
        std::string first_word;
        std::istringstream ins(raw_line);
        ins >> first_word;
        if (first_word[0] == '#') {
          if (debug) {
            std::cerr << "[debug] bxdecay0::dbd_gA::_load_tabulated_probability_: Skip comment line." << raw_line<< std::endl;
          }
          continue;
        }
	}
	std::vector<double> args;
	std::vector<double> front_back;
	for(;line_ins >> line;){
	  if(count == 0)
	    _pimpl_->tab_pdf.energies.push_back(atof(line.c_str()));

	  else args.push_back(atof(line.c_str()));
	}
	count++;
	if(count == 1) continue;
	front_back.push_back(args.front());
	front_back.push_back(args.back());
	_pimpl_->tab_pdf.bound.push_back(front_back);

	_pimpl_->tab_pdf.save_probability.push_back(args);
	

	if (f_tab_pdf.eof()) {
	  if (debug) {
	    std::cerr << "[debug] bxdecay0::dbd_gA::_load_tabulated_probability_: End of parsing input." << std::endl;
	  }
	  break;
	}
	}	
	  
	} //end inverse transform method
	else {
          throw std::logic_error("bxdecay0::dbd_gA::_load_tabulated_probability_: Invalid E sampling mode!");
        }    
    }
     if (debug) {
	   std::cerr << "[debug] bxdecay0::dbd_gA::_load_tabulated_probability_: Number of probability sampling points = "
	   << std::to_string(_pimpl_->tab_pdf.prob_samples) << std::endl;
	   std::cerr << "[debug] bxdecay0::dbd_gA::_load_tabulated_probability_: Energy sampling step = "
	   << std::to_string(_pimpl_->tab_pdf.energy_step) << " MeV" << std::endl;
	   std::cerr << "[debug] bxdecay0::dbd_gA::_load_tabulated_probability_: Minimum sampled energy = "
	   << std::to_string(_pimpl_->tab_pdf.e_min[0]) << " MeV" << std::endl;
	   std::cerr << "[debug] bxdecay0::dbd_gA::_load_tabulated_probability_: Maximum sampled energy = "
	   << std::to_string(_pimpl_->tab_pdf.e_max[0]) << " MeV" << std::endl;
	   std::cerr << "[debug] bxdecay0::dbd_gA::_load_tabulated_probability_: E sampling is parsed." << std::endl;
	   }
     return;
  }
   
  void dbd_gA::reset()
  {
    if (!is_initialized()) {
      throw std::logic_error("bxdecay0::dbd_gA::initialize: Generator is not initialized!");
    }
    _initialized_ = false;

    // Clean the working structure:
    _pimpl_.reset();
    _tabulated_pdf_file_path_.clear();
    
    // Reset configuration:
    _nuclide_.clear();
    _process_ = PROCESS_UNDEF;
    _shooting_ = SHOOTING_UNDEF;
    return;
  }

  void dbd_gA::print(std::ostream & out_,
                    const std::string & title_,
                    const std::string & indent_) const
  {
    if (!title_.empty()) {
      out_ << indent_ << title_ << "\n";
    }
    static const std::string tag = "|-- ";
    static const std::string last_tag = "`-- ";
    static const std::string skip_tag = "|   ";
    out_ << indent_ << tag << "Nuclide : '" << _nuclide_ << "'" << std::endl;
    out_ << indent_ << tag << "Process : '" << process_label(_process_) << "'" <<  std::endl;
    out_ << indent_ << tag << "Tabulated p.d.f file path : '" << _tabulated_pdf_file_path_ << "'" <<  std::endl;
    if (is_initialized()) {
      out_ << indent_ << tag << "Tabulated p.d.f : " << std::endl;
      out_ << indent_ << skip_tag << tag << "esum(max) = " << _pimpl_->tab_pdf.esum_max << " MeV" << std::endl;
      for (int i = 0; i < 2; i++) {
        out_ << indent_ << skip_tag << tag << "e" << i+1 << "(nsamples) = " << _pimpl_->tab_pdf.e_nsamples[i] << std::endl;
        out_ << indent_ << skip_tag << tag << "e" << i+1 << "(min) = " << _pimpl_->tab_pdf.e_min[i] << " MeV" << std::endl;
        out_ << indent_ << skip_tag << tag << "e" << i+1 << "(max) = " << _pimpl_->tab_pdf.e_max[i] << " MeV" << std::endl;
      }
      out_ << indent_ << skip_tag << last_tag << "Prob(max) = " << _pimpl_->tab_pdf.prob_max << std::endl;
      if( _pimpl_->tab_pdf.e_sampling[0] != tabulated_pdf_type::SAMPLING_CUMULATIVE){
      out_ << indent_ << tag << "P.d.f Interpolator : " << std::endl;
      out_ << indent_ << skip_tag << last_tag << "Name : '" << gsl_interp2d_name(_pimpl_->pdf_interpolator.interp) << "'" << std::endl;
      }
    }
    out_ << indent_  << last_tag << "Initialized : " << std::boolalpha << _initialized_ << std::endl;
    
    return;
  }

  void dbd_gA::shoot_e1_e2(i_random & prng_, double & e1_, double & e2_)
  {
    e1_ = std::numeric_limits<double>::quiet_NaN();
    e2_ = std::numeric_limits<double>::quiet_NaN();
    if (_shooting_ == SHOOTING_REJECTION) {
      _shoot_rejection_(prng_, e1_, e2_);
    }
    if (_shooting_ == SHOOTING_INVERSE_TRANSFORM_METHOD) {
      _shoot_inverse_transform_method_(prng_, e1_, e2_);
    }
    
    return;
  }

  void dbd_gA::shoot_cos_theta(i_random & prng_, const double e1_, const double e2_, double & cos12_, event & ev1_)
  { 
    static const double emass = decay0_emass(); //MeV
    static const double PI = 3.14159265;
    static const double twopi = 2. * PI;
    double p1 = std::sqrt(e1_ * (e1_ + 2. * emass));
    double p2 = std::sqrt(e2_ * (e2_ + 2. * emass));
    double b1 = p1 / (e1_ + emass);
    double b2 = p2 / (e2_ + emass);
    // sampling the angles with angular correlation
    double a = 1.;
    double b = -b1 * b2;
    double c = 0.;
    
    double romaxt = a + std::abs(b) + c;
    double phi1, phi2;
    double ctet1, ctet2;
    double stet1, stet2;
    do {
      phi1 = twopi * prng_();
      ctet1 = 1. - 2. * prng_();
      stet1 = std::sqrt(1. - ctet1 * ctet1);
      phi2 = twopi * prng_();
      ctet2 = 1. - 2. * prng_();
      stet2 = std::sqrt(1. - ctet2 * ctet2);
      cos12_ = ctet1 * ctet2 + stet1 * stet2 * std::cos(phi1 - phi2);
    }
    while (romaxt * prng_() > a + b * cos12_ + c * std::pow(cos12_,2));
    
    _pimpl_->tab_pdf.beta =+ b1*b2*_pimpl_->tab_pdf.proba;
    _pimpl_->tab_pdf.sum_proba =+_pimpl_->tab_pdf.proba;
    // Clear the target event
    ev1_.reset();
    ev1_.set_generator("dbd_gA"); // Useful information (not mandatory)
    ev1_.set_time(0.0); // Default decay time set to zero (unit: second)
    particle particle;
    particle.set_time(0.);
    particle.set_code(ELECTRON);
    particle.set_momentum(p1 * stet1 * std::cos(phi1),
			  p1 * stet1 * std::sin(phi1),
			  p1 * ctet1);
    ev1_.add_particle(particle);
    // Second electron/positron :
    particle.set_momentum(p2 * stet2 * std::cos( phi2),
			  p2 * stet2 * std::sin(phi2),
			  p2 * ctet2);
    ev1_.add_particle(particle);
    return;
  }
   
  void dbd_gA::_shoot_rejection_(i_random & prng_, double & e1_, double & e2_)
  {
    while (true) {
      double r1 = prng_();
      double r2 = prng_();
      if (r1 + r2 > 1.0) {
        r1 = 1.0 - r1;
        r2 = 1.0 - r2;
      }
      double e1 = _pimpl_->tab_pdf.e_samples[0].front() + r1 * (_pimpl_->tab_pdf.e_samples[0].back() - _pimpl_->tab_pdf.e_samples[0].front());
      double e2 = _pimpl_->tab_pdf.e_samples[1].front() + r2 * (_pimpl_->tab_pdf.e_samples[1].back() - _pimpl_->tab_pdf.e_samples[1].front());
      double ptest = prng_() * _pimpl_->tab_pdf.esum_max * 1.00001;
      double p = gsl_interp2d_eval(_pimpl_->pdf_interpolator.interp,
                                   _pimpl_->tab_pdf.e_samples[0].data(),
                                   _pimpl_->tab_pdf.e_samples[1].data(),
                                   _pimpl_->tab_pdf.prob.data(),
                                   e1,
                                   e2,
                                   _pimpl_->pdf_interpolator.xacc,
                                   _pimpl_->pdf_interpolator.yacc);
      if (ptest < p and (e1 + e2 < _pimpl_->tab_pdf.esum_max)) {
        e1_ = e1;
        e2_ = e2;
        break;
      }
    }
    return;
  }

  void dbd_gA::_shoot_inverse_transform_method_ (i_random & prng_, double & e1_, double & e2_)
  {
    double rand = prng_();
    int count =0;
    for(unsigned int i=0; i< _pimpl_->tab_pdf.bound.size(); i++)
	  {
	    if((fabs(rand - _pimpl_->tab_pdf.bound[i][0]) <= 5*pow(10,-7)) ||( fabs(rand - _pimpl_->tab_pdf.bound[i][1])<= 5*pow(10,-7)))
	    {
	      if((rand - _pimpl_->tab_pdf.bound[i][0] <= 5* pow(10,-7))){
		e1_ = _pimpl_->tab_pdf.energies[i];
		e2_ = _pimpl_->tab_pdf.energies[0];
		_pimpl_->tab_pdf.proba = rand;
		count ++;
	      _pimpl_->tab_pdf.iteration++;
	      break;
	      }
	      else if((rand - _pimpl_->tab_pdf.bound[i][1] <=  5*pow(10,-7))){
		e1_ =  _pimpl_->tab_pdf.energies[i];
		e2_ =  _pimpl_->tab_pdf.energies[_pimpl_->tab_pdf.save_probability[i].size()-1];
		_pimpl_->tab_pdf.proba = rand;
		count ++;
		_pimpl_->tab_pdf.iteration++;

		break;
	      }
	      else  throw std::logic_error("bxdecay0::dbd_gA::_shoot_inverse_transform_method_: An error in selection!");

	      }
	    else if ((rand > _pimpl_->tab_pdf.bound[i][0] && rand < _pimpl_->tab_pdf.bound[i][1])){
	      double diff = fabs(rand - _pimpl_->tab_pdf.save_probability[i][0]);
	      double p1 = _pimpl_->tab_pdf.save_probability[i][0];
	      unsigned int i_iter, j_iter;
	      for(unsigned int j=1; j< _pimpl_->tab_pdf.save_probability[i].size(); j++){
		if ( fabs(rand - _pimpl_->tab_pdf.save_probability[i][j]) < diff){
		  diff =fabs(rand - _pimpl_->tab_pdf.save_probability[i][j]);
		  p1 = _pimpl_->tab_pdf.save_probability[i][j];
		  i_iter = i;
		  j_iter = j;
		}
		else if(j == _pimpl_->tab_pdf.save_probability[i].size() -1 )
		  {
		    e1_ =  _pimpl_->tab_pdf.energies[i_iter];
		    e2_ =  _pimpl_->tab_pdf.energies[j_iter];
		    _pimpl_->tab_pdf.proba = _pimpl_->tab_pdf.save_probability[i_iter][j_iter];
		    _pimpl_->tab_pdf.iteration++;
		    count ++;
		    break;
		  }
		else continue;
	      }
	    }
	    if(count !=0) break;
	    }
    
    //std::cout << "count = " << count << "  iteration = " <<  _pimpl_->tab_pdf.iteration << std::endl;

  }
  
  // static
  /*void dbd_gA::export_to_event(i_random & prng_,
                               const double e1_,
                               const double e2_,
                               const double cos12_,
                               event & ev_)
  {
    
    // Clear the target eventd
    ev_.reset();
    ev_.set_generator("dbd_gA"); // Useful information (not mandatory)
    ev_.set_time(0.0); // Default decay time set to zero (unit: second)
    particle particle;
    particle.set_time(0.);
    particle.set_code(ELECTRON);
    particle.set_momentum(p1 * stet1 * std::cos(phi1),
			  p1 * stet1 * std::sin(phi1),
			  p1 * ctet1);
    ev_.add_particle(particle);

    // Second electron/positron :
    particle.set_momentum(p2 * stet2 * std::cos( phi2),
			  p2 * stet2 * std::sin(phi2),
			  p2 * ctet2);
    ev_.add_particle(particle);
    
    // Fill the event with two beta particles and the proper
    // kinematics and randomize the emission direction...
    // See "bxdecay0/event.h" and "bxdecay0/particle.h".

    return;
    }*/


} // end of namespace bxdecay0
