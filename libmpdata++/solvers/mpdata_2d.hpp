/** @file
* @copyright University of Warsaw
* @section LICENSE
* GPLv3+ (see the COPYING file or http://www.gnu.org/licenses/)
*/

#pragma once

#include <array>

#include <libmpdata++/formulae/mpdata/formulae_mpdata_2d.hpp>
#include <libmpdata++/formulae/donorcell_formulae.hpp>
#include <libmpdata++/solvers/detail/solver_2d.hpp>

namespace libmpdataxx
{
  namespace solvers
  {
    using namespace arakawa_c;

    template<typename real_t, int n_iters, int n_eqs = 1, int halo = formulae::mpdata::halo>
    class mpdata_2d : public detail::solver_2d<
      real_t,
      2,
      n_eqs,
      formulae::mpdata::n_tlev,
      detail::max(halo, formulae::mpdata::halo)
    >
    {
      static_assert(n_iters > 0, "n_iters <= 0");

      using parent_t = detail::solver_2d<
        real_t,
        2,
        n_eqs,
        formulae::mpdata::n_tlev, 
        detail::max(halo, formulae::mpdata::halo)
      >;

      static const int n_tmp = n_iters > 2 ? 2 : 1;

      // member fields
      std::array<arrvec_t<typename parent_t::arr_t>*, n_tmp> tmp;
      rng_t im, jm;

      protected:

      // method invoked by the solver
      void advop(int e)
      {
	for (int iter = 0; iter < n_iters; ++iter) 
	{
	  if (iter == 0) 
	    formulae::donorcell::op_2d(
	      this->mem->psi[e], 
	      this->n[e], this->mem->C, this->i, this->j);
	  else
	  {
	    this->cycle(e);
            this->mem->barrier();
	    this->bcxl->fill_halos_sclr(this->mem->psi[e][this->n[e]], this->j^halo); // TODO: two xchng calls? (without barriers)
	    this->bcxr->fill_halos_sclr(this->mem->psi[e][this->n[e]], this->j^halo);
	    this->bcyl->fill_halos_sclr(this->mem->psi[e][this->n[e]], this->i^halo);
	    this->bcyr->fill_halos_sclr(this->mem->psi[e][this->n[e]], this->i^halo);
            this->mem->barrier();

	    // choosing input/output for antidiff C
            const arrvec_t<typename parent_t::arr_t>
	      &C_unco = (iter == 1) 
		? this->mem->C 
		: (iter % 2) 
		  ? *tmp[1]  // odd iters
		  : *tmp[0], // even iters
	      &C_corr = (iter  % 2) 
		? *tmp[0]    // odd iters
		: *tmp[1];   // even iters

	    // calculating the antidiffusive C 
	    C_corr[0](this->im+h, this->j) = 
	      formulae::mpdata::antidiff<0>(
		this->mem->psi[e][this->n[e]], 
		this->im, this->j, C_unco
	      );

	    C_corr[1](this->i, this->jm+h) = 
	      formulae::mpdata::antidiff<1>(
              this->mem->psi[e][this->n[e]], 
              this->jm, this->i, C_unco
	    );
 
            this->mem->barrier();
	    this->bcyl->fill_halos_sclr(C_corr[0], this->i^h); // TODO: one xchng?
	    this->bcyr->fill_halos_sclr(C_corr[0], this->i^h);
	    this->bcxl->fill_halos_sclr(C_corr[1], this->j^h); // TODO: one xchng?
	    this->bcxr->fill_halos_sclr(C_corr[1], this->j^h);
            this->mem->barrier();

	    // donor-cell call 
	    formulae::donorcell::op_2d(this->mem->psi[e], 
	      this->n[e], C_corr, this->i, this->j); // TODO: doing antidiff,upstream,antidiff,upstream (for each dimension separately) could help optimise memory consumption!
	  }
	}
      }

      public:

      struct params_t {};

      // ctor
      mpdata_2d(
        typename parent_t::ctor_args_t args,
        const params_t &
      ) : 
	parent_t(args),
	im(args.i.first() - 1, args.i.last()),
	jm(args.j.first() - 1, args.j.last())
      {
	for (int n = 0; n < n_tmp; ++n)
          tmp[n] = &args.mem->tmp[__FILE__][n];
      }

      // memory allocation (to be called once per shared-mem node)
      static void alloc(
        typename parent_t::mem_t *mem, 
        const int nx, const int ny
      )   
      {   
        parent_t::alloc(mem, nx, ny);
        for (int n = 0; n < n_tmp; ++n) 
          parent_t::alloc_tmp_vctr(mem, nx, ny, __FILE__);
      }   
    };
  }; // namespace solvers
}; // namespace libmpdataxx
