/** @file     unit.c
 *  @brief    unit tests for libforth interpreter public interface
 *  @author   Richard Howe (2015)
 *  @license  LGPL v2.1 or Later 
 *            <https://www.gnu.org/licenses/old-licenses/lgpl-2.1.en.html> 
 *  @email    howe.r.j.89@gmail.com 
 *  @todo     The core unit testing functionality should be made into a
 *            library so it can be used again 
 *  @todo     Make this file literate! **/

/*** module to test ***/
#include "libforth.h"
/**********************/

#include <assert.h>
#include <setjmp.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*** very minimal test framework ***/

static unsigned passed, failed;
static double timer;
static clock_t start_time, end_time;
static time_t rawtime;

int color_on = 0, jmpbuf_active = 0, is_silent = 0;
jmp_buf current_test;
unsigned current_line = 0;
int current_result = 0;
const char *current_expr;

static const char *reset(void)  { return color_on ? "\x1b[0m"  : ""; }
static const char *red(void)    { return color_on ? "\x1b[31m" : ""; }
static const char *green(void)  { return color_on ? "\x1b[32m" : ""; }
static const char *yellow(void) { return color_on ? "\x1b[33m" : ""; }
static const char *blue(void)   { return color_on ? "\x1b[34m" : ""; }

static int unit_tester(int test, const char *msg, unsigned line) 
{
	if(test) {
	       	passed++; 
		if(!is_silent)
			printf("      %sok%s:\t%s\n", green(), reset(), msg); 
	} else { 
		failed++;
	       	if(!is_silent)
			printf("  %sFAILED%s:\t%s (line %d)\n", red(), reset(), msg, line);
	}
	return test;
}

static void print_statement(char *stmt) 
{
	if(!is_silent)
		printf("   %sstate%s:\t%s\n", blue(), reset(), stmt);
}

static void print_must(char *must) 
{
	if(!is_silent)
		printf("    %smust%s:\t%s\n", blue(), reset(), must);
}

static void print_note(char *name) 
{ 
	if(!is_silent)
		printf("%s%s%s\n", yellow(), name, reset()); 
}

#define MAX_SIGNALS (256)
static char *(sig_lookup[]) = { /*List of C89 signals and their names*/
	[SIGABRT]       = "SIGABRT",
	[SIGFPE]        = "SIGFPE",
	[SIGILL]        = "SIGILL",
	[SIGINT]        = "SIGINT",
	[SIGSEGV]       = "SIGSEGV",
	[SIGTERM]       = "SIGTERM",
	[MAX_SIGNALS]   = NULL
};

static int caught_signal;
static void print_caught_signal_name(void) 
{
	char *sig_name = "UNKNOWN SIGNAL";
	if((caught_signal > 0) && (caught_signal < MAX_SIGNALS) && sig_lookup[caught_signal])
		sig_name = sig_lookup[caught_signal];
	if(!is_silent)
		printf("caught %s (signal number %d)\n", sig_name, caught_signal);\
}

static void sig_abrt_handler(int sig) 
{ /* catches assert() from within functions being exercised */
	caught_signal = sig;
	if(jmpbuf_active) {
		jmpbuf_active = 0;
		longjmp(current_test, 1);
	}
}

/**@brief Advance the test suite by testing and executing an expression. This
 *        framework can catch assertions that have failed within the expression
 *        being tested.
 * @param EXPR The expression should yield non zero on success **/
#define test(EXPR)\
	do {\
		current_line = __LINE__;\
		current_expr = #EXPR;\
		signal(SIGABRT, sig_abrt_handler);\
		if(!setjmp(current_test)) {\
			jmpbuf_active = 1;\
			current_result = unit_tester( ((EXPR) != 0), current_expr, current_line);\
		} else {\
			print_caught_signal_name();\
			current_line = unit_tester(0, current_expr, current_line);\
			signal(SIGABRT, sig_abrt_handler);\
		}\
		signal(SIGABRT, SIG_DFL);\
		jmpbuf_active = 0;\
	} while(0)

#define must(EXPR)\
	do {\
		print_must(#EXPR);\
		test(EXPR);\
		if(!current_result) {\
			exit(-1);\
		}\
	} while(0)

/**@brief print out and execute a statement that is needed to further a test
 * @param STMT A statement to print out (stringify first) and then execute**/
#define state(STMT) do{ print_statement( #STMT ); STMT; } while(0);

/**@brief As signals are caught (such as those generated by abort()), we exit
 *        the unit test function by returning from it instead. */
#define return_if(EXPR) if((EXPR)) { printf("unit test framework failed on line '%d'\n", __LINE__); return -1;}

static int unit_test_start(const char *unit_name) 
{
	time(&rawtime);
	if(signal(SIGABRT, sig_abrt_handler) == SIG_ERR) {
		fprintf(stderr, "signal handler installation failed");
		return -1;
	}
	start_time = clock();
	if(!is_silent)
		printf("%s unit tests\n%sbegin:\n\n", unit_name, asctime(localtime(&rawtime)));
	return 0;
}

static unsigned unit_test_end(const char *unit_name) 
{
	end_time = clock();
	timer = ((double) (end_time - start_time)) / CLOCKS_PER_SEC;
	if(!is_silent)
		printf("\n\n%s unit tests\npassed  %u/%u\ntime    %fs\n", unit_name, passed, passed+failed, timer);
	return failed;
}

/*** end minimal test framework ***/

static char usage[] = "\
libforth unit test framework\n\
\n\
	usage: %s [-h] [-c] [-k] [-s] [-]\n\
\n\
	-h	print this help message and exit (unsuccessfully so tests do not pass)\n\
	-c	turn colorized output on (forced on)\n\
	-k	keep any temporary file\n\
	-s	silent mode\n\
	-       stop processing command line arguments\n\
\n\
This program executes are series of tests to exercise the libforth library. It\n\
will return zero on success and non zero on failure. The tests and results will\n\
be printed out as executed.\n\
\n";

static int keep_files = 0;

int main(int argc, char **argv) {
	int i;
	for(i = 1; i < argc && argv[i][0] == '-'; i++)
		switch(argv[i][1]) {
			case '\0':
				goto done;
			case 's':
				is_silent = 1;
				break;
			case 'h': 
				fprintf(stderr, usage, argv[0]);
				return -1;
			case 'c': 
				color_on = 1;
				break;
			case 'k': 
				keep_files = 1; 
				break;
			default:
				fprintf(stderr, "invalid argument '%s'\n", argv[i]);
				fprintf(stderr, usage, argv[0]);
				return -1;
		}
done:
	unit_test_start("libforth");
	{
		/** @todo The entire external API needs testing, as well as
		 * parts of the internals
		 *
		 * The following functions still need testing:
		 * 	- int forth_dump_core(forth_t *o, FILE *dump);
		 * 	- void forth_set_file_output(forth_t *o, FILE *out);
		 * 	- void forth_set_args(forth_t *o, int argc, char **argv);
		 *	- int main_forth(int argc, char **argv); 
		 **/

		FILE *core;
		forth_cell_t here;
		forth_t *f;
		print_note("libforth.c");
		state(f = forth_init(MINIMUM_CORE_SIZE, stdin, stdout));
		must(f);
		state(core = fopen("unit.core", "wb"));
		must(core);

		/* test setup, simple tests of push/pop interface */
		test(0 == forth_stack_position(f));
		test(forth_eval(f, "here ")  >= 0);
		state(here = forth_pop(f));
		state(forth_push(f, here));
		test(forth_eval(f, "2 2 + ") >= 0);
		test(forth_pop(f) == 4);
		/* define a word, call that word, pop result */
		test(!forth_find(f, "unit-01"));
		test(forth_eval(f, ": unit-01 69 ; unit-01 ") >= 0);
		test(forth_find(f,  "unit-01"));
		test(!forth_find(f, "unit-01 ")); /* notice the trailing space */
		test(forth_pop(f) == 69);
		test(1 == forth_stack_position(f)); /* "here" still on stack */

		/* constants */
		test(forth_define_constant(f, "constant-1", 0xAA0A) >= 0);
		test(forth_define_constant(f, "constant-2", 0x5055) >= 0);
		test(forth_eval(f, "constant-1 constant-2 or") >= 0);
		test(forth_pop(f) == 0xFA5F);

		/* string input */
		state(forth_set_string_input(f, " 18 2 /"));
		test(forth_run(f) >= 0);
		test(forth_pop(f) == 9);
		state(forth_set_file_input(f, stdin));

		/* save core for later tests */
		test(forth_save_core(f, core) >= 0);
		state(fclose(core));

		/* more simple tests of arithmetic */
		state(forth_push(f, 99));
		state(forth_push(f, 98));
		test(forth_eval(f, "+") >= 0);
		test(forth_pop(f) == 197);
		test(1 == forth_stack_position(f)); /* "here" still on stack */
		test(here == forth_pop(f));
		state(forth_free(f));

	}
	{
		/* Test the persistence of word definitions across core loads*/
		FILE *core;
		forth_t *f;
		state(core = fopen("unit.core", "rb"));
		must(core);

		/* test that definitions persist across core dumps */
		state(f = forth_load_core(core));
		/* stack position does no persist across loads, this might
		 * change, but is the current functionality */
		test(0 == forth_stack_position(f)); 
		must(f);
		/* the word "unit-01" was defined earlier */
		test(forth_find(f, "unit-01"));
		test(forth_eval(f, "unit-01 constant-1 *") >= 0);
		test(forth_pop(f) == 69 * 0xAA0A);
		test(0 == forth_stack_position(f));

		state(forth_free(f));
		state(fclose(core));
		if(!keep_files)
			state(remove("unit.core"));
	}
	{
		/* test the built in words, there is a set of built in words
		 * that are defined in the interpreter, these must be tested 
		 *
		 * The following words need testing:
		 * 	[ ] :noname 
		 * 	'\n' ')' cr :: 
		 */
		forth_t *f;
		state(f = forth_init(MINIMUM_CORE_SIZE, stdin, stdout));
		must(f);

		/* here we test if...else...then statements and hex conversion,
		 * this also tests >mark indirectly */
		test(forth_eval(f, ": if-test if 0x55 else 0xAA then ;") >= 0);
		test(forth_eval(f, "0 if-test") >= 0);
		test(forth_pop(f) == 0xAA);
		state(forth_push(f, 1));
		test(forth_eval(f, "if-test") >= 0);
		test(forth_pop(f) == 0x55);

		/* simple loop tests */
		test(forth_eval(f, " : loop-test begin 1 + dup 10 u> until ;") >= 0);
		test(forth_eval(f, " 1 loop-test") >= 0);
		test(forth_pop(f) == 11);
		test(forth_eval(f, " 39 loop-test") >= 0);
		test(forth_pop(f) == 40);

		/* rot and comments */
		test(forth_eval(f, " 1 2 3 rot ( 1 2 3 -- 2 3 1 )") >= 0);
		test(forth_pop(f) == 1);
		test(forth_pop(f) == 3);
		test(forth_pop(f) == 2);

		/* -rot */
		test(forth_eval(f, " 1 2 3 -rot ") >= 0);
		test(forth_pop(f) == 2);
		test(forth_pop(f) == 1);
		test(forth_pop(f) == 3);

		/* nip */
		test(forth_eval(f, " 3 4 5 nip ") >= 0);
		test(forth_pop(f) == 5);
		test(forth_pop(f) == 3);

		/* allot */
		test(forth_eval(f, " here 32 allot here swap - ") >= 0);
		test(forth_pop(f) == 32);

		/* tuck */
		test(forth_eval(f, " 67 23 tuck ") >= 0);
		test(forth_pop(f) == 23);
		test(forth_pop(f) == 67);
		test(forth_pop(f) == 23);


		state(forth_free(f));
	}
	{
		/* test the forth interpreter internals */
		forth_t *f;
		state(f = forth_init(MINIMUM_CORE_SIZE, stdin, stdout));
		must(f);

		/* base should be set to zero, this is a special value
		 * that allows hexadecimal, octal and decimal to be read
		 * in if formatted correctly;
		 * 	- hex     0x[0-9a-fA-F]*
		 * 	- octal   0[0-7]*
		 * 	- decimal [1-9][0-9]* 
		 */
		test(forth_eval(f, " base @ 0 = ") >= 0);
		test(forth_pop(f));

		/* the invalid flag should not be set */
		test(forth_eval(f, " `invalid @ 0 = ") >= 0);
		test(forth_pop(f));

		/* source id should be -1 (reading from string) */
		test(forth_eval(f, " `source-id @ -1 = ") >= 0);
		test(forth_pop(f));

		state(forth_free(f));
	}
	return !!unit_test_end("libforth");
}
