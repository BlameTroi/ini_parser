	    ==============================================
			  Simply Parse in C
	    A Working INI File Parser in ~150 Lines of C99
		 from the paper by Chloe Kudryavtsev
	    ==============================================



Introduction and Overview
-------------------------

Chloe Kudryavtsev (CK) wrote a paper in July of 2023 to demonstrate
that parsers aren't really all that complex. The original is at
https://pencil.toast.cafe/bunker-labs/simply-parse-in-c (as of October
2024).

This repository is my read- and work-through of the paper. I don't
expect to do anything meaningfully original here, but my stuff is
always public domain (see License below). CK placed no license or
copyright notice on the paper, nor did I find one elsewhere on her
site.

Troy Brumley, blametroi@gmail.com, October 2024.

So let it be written,
So let it be done.



License
-------

Either public domain under the UNLICENSE, or MIT as you like.



Dependencies and Tooling
------------------------

This is all built on a MacBook M2 using Apple's current standard
Clang. I'm transitioning from GNU Make to CMake and Ninja. Formatting
is done every time a file is saved using Artistic Style. I edit with
Emacs using Eglot, Treesitter, and Clangd. I compile with "-Wall
--pedantic-errors -Werror -std=c18" for all builds. Debug builds add
"-g3 -fsanitize=address".



Code Notes
----------

The original code as pulled from the paper is in a separate directory
from my experiments with it. My editor formatters will change tabbing
and might move some braces around, but the code is meant to be
identical to the original. Some minor bugs were found and fixed, these
are noted by comments flagged 'txb'.

I'll did my work through in another directory and then redid most of
the code to make something suitable for inclusion in my libraries. in
so doing i tightened up some behavior (missing keys or values were
ignored in the original). 

The original was advertised as taking about 150 lines. 'cloc' reports
157 lines so mission accomplished.

The workthru/rewrite is reported as having 206 lines of code. not 
bad at all.



To Do & Bugs
------------

* *fixed* Missing keys in a 'key = value' pair throw the parser out
  of sync. Now a negative count is returned to the client. Negative
  means some error occured.

* Missing values also throw the parser out of sync.
