/** @file
* @copyright University of Warsaw
* @section LICENSE
* GPLv3+ (see the COPYING file or http://www.gnu.org/licenses/)
*/

#pragma once

#include "detail/solver_2d.hpp"
#include "../formulae/donorcell_formulae.hpp"

namespace advoocat
{
  namespace solvers
  {
    template<class mem_t, int halo = formulae::donorcell::halo>
    class donorcell_2d : public detail::solver_2d<
      mem_t, 
      formulae::donorcell::n_tlev,
      detail::max(halo, formulae::donorcell::halo)
    > 
    {
      using parent_t = detail::solver_2d<
        mem_t, 
        formulae::donorcell::n_tlev, 
        detail::max(halo, formulae::donorcell::halo)
      >;

      void advop(int e)
      {
        formulae::donorcell::op_2d(
          this->mem.psi[e], this->mem.n[e], this->mem.C, this->i, this->j
        );
      }

      public:

      struct params_t {};

      // ctor
      donorcell_2d(
        mem_t &mem, 
        typename parent_t::bc_p &bcx,
        typename parent_t::bc_p &bcy,
        const rng_t &i, 
        const rng_t &j, 
        const params_t &
      ) :
        parent_t(mem, bcx, bcy, i, j)
      {}  
    };
  }; // namespace solvers
}; // namespace advoocat

