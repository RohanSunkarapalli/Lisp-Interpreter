# Lisp-Interpreter

Developed a Lisp Interpreter using C++

This is new dialect of lisp called Yisp which is quite like common lisp.

Brief info about my code -

The interpreter processes an input from the user through the method Simplelispinterpreter::Process().  It breaks the input into tokens that are in turns parsed into lisp expressions.  All lisp expressions implement the interface LispExprInterface.  So, they all implement the method that returns their respective types (number, string, function ...) and their string representations. 

The interpreter then evaluates an expression through the method Simplelispinterpreter::Evaluate().  There are some simple expressions that evaluate to themselves (string, number, ...), and others that need further processing. 

The SymbolExpr class that represents a symbol needs a little bit of attention.  A symbol is case insensitive.  And the symbol hello and HELLO represent the same physical object.  A symbol in the system is implemented through the static method SymbolExpr::GetInstance().  It always returns a unique object of the given name. 

The nil or () object is represented by the NilExpr class.  It only has one single instance accessed through the NilExpr::GetInstance() method.  We define the global constant kNil to represent a pointer to it.  The same is also true for the T object.


What to Look at/ Advantages –

•	There are also two kinds of functions in our lisp implementation: primitive ones (PrimExpr) and user-defined ones (FunExpr). 

•	A primitive function takes Lisp objects as parameters but is implemented natively in C++.  For example, the + function is implemented through a call to the PrimPlus() function.  All the primitives function are defined when creating an interpreter through the method Simplelispinterpreter::InitGlobalEnv(). 

•	On the other hand, the body of a user-defined function (through DEFINE) is evaluated in a new environment created where its parameters are binded to its arguments.  All this machinery could be found in the Simplelispinterpreter::Apply() method. 

•	We have implemented AND? as a primitive function but we could have implemented it in Lisp itself as follows: 

         (define and? (lst) 
             (cond (nil? lst) t 
                       (nil? (car lst)) () 
                       t (and? (cdr lst)))) 

So, we should probably implement a bunch of library functions inside library.lisp file and evaluate its content at start time before processing inputs from the user.  This way, we only need to implement the core features in C++.

•	We can define our own functions which can perform according to the way we implement it

•	Apart from this, I believe this interpreter can perform all the keywords according to your dialect which have been stated in requirements.

•	It has got wide range of testcases designed which work well.

•	We can also implement lambda, eval, quote, mapcar additionally using this interpreter.

•	This interpreter performs good as expected and I was successful to implement all test cases perfectly producing no error.


What to Not Look at/ Disadvantages –

•	The weakest parts of the unit tests are probably the "TEST 2 - NUMERICAL OPERATIONS" and the "TEST 8 - NUMERICAL COMPARISON" parts. For simplicity, we represent a number as a float.  But comparing two floats doesn't really make sense unless we introduce a margin of error.  So, these tests are naive. 

•	In "TEST 11 - USER DEFINED FUNCTIONS" we define the function pair as (define pair (car cdr) (cons car cdr)).

•	Notice the name of the parameters are the same as primitive functions.  But it works correctly since we evaluate the function in a newly defined environment that points to the old one.  So, these definitions are not overwritten. 

•	Finally, in TEST 12 and TEST 13, recursion and higher order functions are fully supported.  Although there's no tail call elimination being done.

•	The execution part, I don’t think it’s a good approach. One good thing I made sure is to write all the testcases in one file and when you execute that file it returns all the outputs of the testcases mentioned in the lisp.test file. So, if we need to check any testcase we can write in that file and better execute it. If not, we need to execute one expression after another expression to see the outputs. Even in the case of defining functions first we need to execute the define statement and then again, we need to run the defined function to check out the output.

•	One major thing is to follow the syntax perfectly else it leads to error.
