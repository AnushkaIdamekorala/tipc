foo(){
    for ITERATE in {1..50}
      do
        echo "$ITERATE"
      done
  }


foo_optimized(){
    for ITERATE in {1..50}
      do
        echo "$ITERATE"
      done
  }
##LICM_test.tip dse_test.tip dce_test.tip  tailCallElem_test.tip ipsccp_test.tip
##for FILE in LICM_test.tip dse_test.tip dce_test.tip  tailCallElem_test.tip ipsccp_test.tip
##  do
  echo "$1"
  time foo $1.tip
  echo "Optimized times"
  time foo_optimized $1.tip
##  done
