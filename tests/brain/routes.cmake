# Retail files stay outside the repository. The scripts contain input only.
function(run_route project ticks keys required)
  execute_process(COMMAND timeout 30 ${EXE} ${GAME} ${project} --ticks ${ticks} --keys "${keys}" --trace --summary
    OUTPUT_VARIABLE A RESULT_VARIABLE R)
  execute_process(COMMAND timeout 30 ${EXE} ${GAME} ${project} --ticks ${ticks} --keys "${keys}" --trace --summary
    OUTPUT_VARIABLE B RESULT_VARIABLE S)
  if(NOT R EQUAL 0 OR NOT S EQUAL 0 OR NOT A STREQUAL B)
    message(FATAL_ERROR "${project}: execution failed or trace is nondeterministic")
  endif()
  foreach(pattern IN LISTS required)
    if(NOT A MATCHES "${pattern}")
      message(FATAL_ERROR "${project}: missing ${pattern}")
    endif()
  endforeach()
endfunction()
# level001 is the title/demo. Space takes its real exit to the first playable project.
run_route(level001 10 "0:57;1:" "ticks 1 exit 3;next level003.prj")
# First playable stage: north, east, north, then jump through Bonus-ring-1.
run_route(level003 310 "0:;110:77;126:;222:75;238:;292:29;306:" "collision 53 74 color 5;state score 500 lives 3")
# Turn east toward Follow-4. Its real collision costs a life while unpowered.
run_route(level003 225 "0:;175:77;191:" "collision 53 159 color 2;global\\[220\\] 3 -> 2;state score 50 lives 2")
