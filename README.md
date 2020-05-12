## Example

addition.ret:
```
#!retranse -i
#meta retranse-dialect A
# **************************************************************************** #
# * This is retranse sample #4                                                 *
# * calculator that adds two arbitrarily long numbers                          *
# *                                                                            *
# * written by Kimon Kontosis                                                  *
# **************************************************************************** #

function d_inc ( (.*) )
{
  15 16 [L] ; 16 17 [L] ; 17 18 [L] ; 18 19 [L] ; 19 20 [L]
  10 11 [L] ; 11 12 [L] ; 12 13 [L] ; 13 14 [L] ; 14 15 [L]
   9 10 [L] ;  8  9 [L] ;  7  8 [L] ;  6  7 [L] ;  5  6 [L]
    4 5 [L] ;  3  4 [L] ;  2  3 [L] ;  1  2 [L] ;  0  1 [L]
}
function d_dec ( (.*) )
{
  0 -1 [L] ; 9 8 [L] ; 8 7 [L] ; 7 6 [L] ; 6 5 [L]
   5 4 [L] ; 4 3 [L] ; 3 2 [L] ; 2 1 [L] ; 1 0 [L]
}

function d_add ( 0 (.*) ) { }
function d_add ( (.*) (.*) )
{
  call l = d_dec ( $(a1) )
  call r = d_inc ( $(a2) )
  reduce call d_add ( $(l) $(r) )
}

function add ( "" (.*) ) { }
function add ( (.*) "" ) { }
function add ( (.*)(.) (.*)(.) )
{
  call X = add ( $(a1) $(a3) )
  cond ( $(a2) == 0 )
    reduce to $(X)$(a4)

  call x = d_add ( $(a2) $(a4) )
  reduce call add ( $(X)0 $(x) )
}

(.*)\+(.*) - [B] { reduce call add ( $1 $2 ) }
```

## Run

```
$ ./retranse -i addition.ret
        100 21
        121
```
