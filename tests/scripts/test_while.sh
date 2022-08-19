#!/bin/bash

source "$( dirname -- "$( readlink -f -- "$0"; )"; )/assert.sh"

suite "while statement should execute as expected"

assertFile "tests/examples/loop/while.dojo" '0
1
2
3
4
5
6
5
4
3
2
1
0'

suite "should report error if missing parens around condition"

assertFileError "tests/examples/loop/error_while_parens.dojo" 

suite "should report error if condition is not an expression"

assertFileError "tests/examples/loop/error_while_expression.dojo"

