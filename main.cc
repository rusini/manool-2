// main.cc

# include "ir.hh"

namespace rsn::opt {
   void update_cfg_preds(proc *);
   bool transform_const_propag(proc *);
}

int main() {
   namespace opt = rsn::opt;

   auto tu = opt::proc::make({1, 0});

   auto b0 = opt::bblock::make(tu), b1 = opt::bblock::make(tu), b2 = opt::bblock::make(tu), b3 = opt::bblock::make(tu);
   auto r0 = opt::vreg::make();

   opt::insn_entry::make(b0, {});
   opt::insn_mov::make(b0, opt::abs::make(123), r0);
   opt::insn_br::make_beq(b0, opt::abs::make(0), opt::abs::make(0), b1, b2);

   opt::insn_jmp::make(b1, b1);

   opt::insn_mov::make(b2, opt::abs::make(123), r0);
   opt::insn_jmp::make(b2, b3);

   opt::insn_ret::make(b3, {r0});

   tu->dump();
   update_cfg_preds(tu);
   transform_const_propag(tu);
   tu->dump();

   return {};
}
