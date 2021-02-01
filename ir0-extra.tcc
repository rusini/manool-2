// ir0-extra.tcc

# ifdef RSN_OPT_EXTRA_PROC
# endif

# ifdef RSN_OPT_EXTRA_VREG
# endif

# ifdef RSN_OPT_EXTRA_BBLOCK
   bblock *bb;
   std::vector<bblock *> preds;
   bool visited;
# endif

# ifdef RSN_OPT_EXTRA_INSN
   bool visited{};
# endif
