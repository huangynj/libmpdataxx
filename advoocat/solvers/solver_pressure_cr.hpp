/** 
  * @file
  * @copyright University of Warsaw
  * @section LICENSE
  * GPLv3+ (see the COPYING file or http://www.gnu.org/licenses/)
  */

#pragma once
#include "detail/solver_pressure_common.hpp"
#include "../formulae/nabla_formulae.hpp" //gradient, diveregnce

namespace advoocat
{
  namespace solvers
  {
    template <class inhomo_solver_t, int u, int w, int tht>
    class pressure_cr : public detail::pressure_solver_common<inhomo_solver_t, u, w, tht>
    {
      public:

      using parent_t = detail::pressure_solver_common<inhomo_solver_t, u, w, tht>;
      using real_t = typename parent_t::real_t;

      typename parent_t::arr_t Phi, err, p_err;
      typename parent_t::arr_t lap_err, lap_p_err; 
      //TODO probably don't need those
      typename parent_t::arr_t tmp_u, tmp_w, tmp_x, tmp_z;
      typename parent_t::arr_t tmp_e1, tmp_e2;

      private:

      void ini_pressure()
      {
	// dt/2 * (Prs-Prs_amb) / rho
	Phi(this->i^this->halo, this->j^this->halo) = real_t(0);
      }

      void pressure_solver_update(real_t dt)
      {
	using namespace arakawa_c;
	using formulae::nabla_op::grad;
	using formulae::nabla_op::div;

	real_t beta = .25;   //TODO
        real_t alpha = 1.;   //TODO
	real_t rho = 1.;     //TODO    
        real_t tmp_den = 1.; //TODO

	int halo = this->halo;
	rng_t i = this->i;
	rng_t j = this->j;

	tmp_u = this->psi(u);
	tmp_w = this->psi(w);

        this->xchng(Phi,   i^halo, j^halo);
        this->xchng(tmp_u, i^halo, j^halo);
	this->xchng(tmp_w, i^halo, j^halo);

	tmp_x(i, j) = rho * tmp_u(i, j) - grad<0>(Phi(i^halo, j^halo), i, j, real_t(1));
	tmp_z(i, j) = rho * tmp_w(i, j) - grad<1>(Phi(i^halo, j^halo), j, i, real_t(1));
     
	this->xchng(tmp_x, i^halo, j^halo);
	this->xchng(tmp_z, i^halo, j^halo);

        err(i, j) = - 1./ rho * div(tmp_x(i^halo,j^halo), tmp_z(i^halo, j^halo), i, j, real_t(1), real_t(1)); //error

        p_err(i ,j) = err(i, j);
        this->xchng(p_err, i^halo, j^halo);
        tmp_e1(i, j) = grad<0>(p_err(i^halo, j^halo), i, j, real_t(1));
        tmp_e2(i, j) = grad<1>(p_err(i^halo, j^halo), j, i, real_t(1));
        this->xchng(tmp_e1, i^halo, j^halo);
        this->xchng(tmp_e2, i^halo, j^halo);
        lap_p_err(i,j) = div(tmp_e1(i^halo,j^halo), tmp_e2(i^halo, j^halo), i, j, real_t(1), real_t(1)); //laplasjan(error)

    std::cerr<<"--------------------------------------------------------------"<<std::endl;

	//pseudo-time loop
	real_t error = 1.;
	while (error > .00001)
	{
          tmp_e1(i,j) = err(i,j) * lap_p_err(i,j);
          tmp_e2(i,j) = lap_p_err(i,j) * lap_p_err(i,j);
          tmp_den = blitz::sum(tmp_e2(i,j));
          if (tmp_den != 0) {beta = - blitz::sum(tmp_e1(i,j))/tmp_den;}
          Phi(i, j) += beta * p_err(i, j);
          err(i, j) += beta * lap_p_err(i, j);

          this->xchng(err, i^halo, j^halo);
          tmp_e1(i, j) = grad<0>(err(i^halo, j^halo), i, j, real_t(1));
          tmp_e2(i, j) = grad<1>(err(i^halo, j^halo), j, i, real_t(1));
          this->xchng(tmp_e1, i^halo, j^halo);
          this->xchng(tmp_e2, i^halo, j^halo);
          lap_err(i,j) = div(tmp_e1(i^halo,j^halo), tmp_e2(i^halo, j^halo), i, j, real_t(1), real_t(1)); //laplasjan(error)
 
          tmp_e1(i,j) = lap_err(i,j) * lap_p_err(i,j);
          if (tmp_den != 0) {alpha = - blitz::sum(tmp_e1(i,j))/tmp_den;}          

          p_err(i, j) *= alpha;
          p_err(i, j) += err(i, j);  
 
          lap_p_err(i,j) *= alpha;
          lap_p_err(i,j) += lap_err(i,j);
 
          error = std::max(std::abs(max(err)), std::abs(min(err)));
std::cerr<<"error "<<error<<std::endl;
          this->iters++;
	}

	//end of pseudo_time loop
	this->xchng(this->Phi, i^halo, j^halo);

	tmp_u(i, j) -= grad<0>(Phi(i^halo, j^halo), i, j, real_t(1));
	tmp_w(i, j) -= grad<1>(Phi(i^halo, j^halo), j, i, real_t(1));

	tmp_u -= this->psi(u);
	tmp_w -= this->psi(w);
      }

      void pressure_solver_apply(real_t dt)
      {
	auto U = this->psi(u);
	auto W = this->psi(w);

	U += tmp_u;
	W += tmp_w;
      }

      public:

      struct params_t : parent_t::params_t { };

      // ctor
      pressure_cr(
	typename parent_t::mem_t *mem,
        typename parent_t::bc_p &bcxl,
        typename parent_t::bc_p &bcxr,
        typename parent_t::bc_p &bcyl,
        typename parent_t::bc_p &bcyr,
	const rng_t &i,
	const rng_t &j,
	const params_t &p
      ) :
	parent_t(mem, bcxl, bcxr, bcyl, bcyr, i, j, p),
        // (i, j)
        lap_err(mem->tmp[std::string(__FILE__)][0][0]),
        lap_p_err(mem->tmp[std::string(__FILE__)][0][1]),
        // (i^hlo, j^hlo))
	err(mem->tmp[std::string(__FILE__)][0][2]),
	tmp_x(mem->tmp[std::string(__FILE__)][0][3]),
	tmp_z(mem->tmp[std::string(__FILE__)][0][4]),
	tmp_u(mem->tmp[std::string(__FILE__)][0][5]),
	tmp_w(mem->tmp[std::string(__FILE__)][0][6]),
	Phi(mem->tmp[std::string(__FILE__)][0][7]),
	tmp_e1(mem->tmp[std::string(__FILE__)][0][8]),
	tmp_e2(mem->tmp[std::string(__FILE__)][0][9]),
	p_err(mem->tmp[std::string(__FILE__)][0][10])
      {}

      static void alloc(typename parent_t::mem_t *mem, const int nx, const int ny)
      {
        parent_t::alloc(mem, nx, ny);

        const std::string file(__FILE__);
        const rng_t i(0, nx-1), j(0, ny-1);
        const int halo = parent_t::halo; 

        // temporary fields
        mem->tmp[file].push_back(new arrvec_t<typename parent_t::arr_t>());
        {
          for (int n=0; n < 2; ++n) 
            mem->tmp[file].back().push_back(new typename parent_t::arr_t(i, j)); 
          for (int n=0; n < 9; ++n) 
            mem->tmp[file].back().push_back(new typename parent_t::arr_t(i^halo, j^halo)); 
        }
      }
    }; 
  }; // namespace solvers
}; // namespace advoocat