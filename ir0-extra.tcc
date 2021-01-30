// opt-temp.tcc

# ifdef RSN_OPT_EXTRA_PROC
# endif

# ifdef RSN_OPT_EXTRA_REG
# endif

# ifdef RSN_OPT_EXTRA_BBLOCK
   bblock *bb;
   std::vector<bblock *> preds;
# endif

# ifdef RSN_OPT_EXTRA_INSN
   bool visited{};
# endif
