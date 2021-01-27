// opt-temp.tcc

# ifdef RSN_OPT_TEMP_PROC
# endif

# ifdef RSN_OPT_TEMP_REG
# endif

# ifdef RSN_OPT_TEMP_BBLOCK
   bblock *bb;
   std::vector<bblock *> preds;
# endif

# ifdef RSN_OPT_TEMP_INSN
   bool visited{};
# endif
