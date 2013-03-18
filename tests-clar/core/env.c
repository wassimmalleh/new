#include "clar_libgit2.h"
#include "fileops.h"
#include "path.h"

#ifdef GIT_WIN32
#define NUM_VARS 5
static const char *env_vars[NUM_VARS] = {
	"HOME", "HOMEDRIVE", "HOMEPATH", "USERPROFILE", "PROGRAMFILES"
};
#else
#define NUM_VARS 1
static const char *env_vars[NUM_VARS] = { "HOME" };
#endif

static char *env_save[NUM_VARS];

static char *home_values[] = {
	"fake_home",
	"f\xc3\xa1ke_h\xc3\xb5me", /* all in latin-1 supplement */
	"f\xc4\x80ke_\xc4\xa4ome", /* latin extended */
	"f\xce\xb1\xce\xba\xce\xb5_h\xce\xbfm\xce\xad",  /* having fun with greek */
	"fa\xe0" "\xb8" "\x87" "e_\xe0" "\xb8" "\x99" "ome", /* thai characters */
	"f\xe1\x9cx80ke_\xe1\x9c\x91ome", /* tagalog characters */
	"\xe1\xb8\x9f\xe1\xba\xa2" "ke_ho" "\xe1" "\xb9" "\x81" "e", /* latin extended additional */
	"\xf0\x9f\x98\x98\xf0\x9f\x98\x82", /* emoticons */
	NULL
};

void test_core_env__initialize(void)
{
	int i;
	for (i = 0; i < NUM_VARS; ++i) {
		const char *original = cl_getenv(env_vars[i]);
#ifdef GIT_WIN32
		env_save[i] = original;
#else
		env_save[i] = original ? git__strdup(original) : NULL;
#endif
	}
}

static void reset_global_search_path(void)
{
	cl_git_pass(git_futils_dirs_set(GIT_FUTILS_DIR_GLOBAL, NULL));
}

static void reset_system_search_path(void)
{
	cl_git_pass(git_futils_dirs_set(GIT_FUTILS_DIR_SYSTEM, NULL));
}

void test_core_env__cleanup(void)
{
	int i;
	char **val;

	for (i = 0; i < NUM_VARS; ++i) {
		cl_setenv(env_vars[i], env_save[i]);
		git__free(env_save[i]);
		env_save[i] = NULL;
	}

	/* these will probably have already been cleaned up, but if a test
	 * fails, then it's probably good to try and clear out these dirs
	 */
	for (val = home_values; *val != NULL; val++) {
		if (**val != '\0')
			(void)p_rmdir(*val);
	}

	/* reset search paths to default */
	reset_global_search_path();
	reset_system_search_path();
}

static void setenv_and_check(const char *name, const char *value)
{
	char *check;

	cl_git_pass(cl_setenv(name, value));

	check = cl_getenv(name);
	cl_assert_equal_s(value, check);
	git__free(check);
}

void test_core_env__0(void)
{
	git_buf path = GIT_BUF_INIT, found = GIT_BUF_INIT;
	char testfile[16], tidx = '0';
	char **val;
	const char *testname = "testfile";
	size_t testlen = strlen(testname);

	strncpy(testfile, testname, sizeof(testfile));
	cl_assert_equal_s(testname, testfile);

	for (val = home_values; *val != NULL; val++) {

		/* if we can't make the directory, let's just assume
		 * we are on a filesystem that doesn't support the
		 * characters in question and skip this test...
		 */
		if (p_mkdir(*val, 0777) != 0) {
			*val = ""; /* mark as not created */
			continue;
		}

		cl_git_pass(git_path_prettify(&path, *val, NULL));

		/* vary testfile name in each directory so accidentally leaving
		 * an environment variable set from a previous iteration won't
		 * accidentally make this test pass...
		 */
		testfile[testlen] = tidx++;
		cl_git_pass(git_buf_joinpath(&path, path.ptr, testfile));
		cl_git_mkfile(path.ptr, "find me");
		git_buf_rtruncate_at_char(&path, '/');

		cl_assert_equal_i(
			GIT_ENOTFOUND, git_futils_find_global_file(&found, testfile));

		setenv_and_check("HOME", path.ptr);
		reset_global_search_path();

		cl_git_pass(git_futils_find_global_file(&found, testfile));

		cl_setenv("HOME", env_save[0]);
		reset_global_search_path();

		cl_assert_equal_i(
			GIT_ENOTFOUND, git_futils_find_global_file(&found, testfile));

#ifdef GIT_WIN32
		setenv_and_check("HOMEDRIVE", NULL);
		setenv_and_check("HOMEPATH", NULL);
		setenv_and_check("USERPROFILE", path.ptr);
		reset_global_search_path();

		cl_git_pass(git_futils_find_global_file(&found, testfile));

		{
			int root = git_path_root(path.ptr);
			char old;

			if (root >= 0) {
				setenv_and_check("USERPROFILE", NULL);
				reset_global_search_path();

				cl_assert_equal_i(
					GIT_ENOTFOUND, git_futils_find_global_file(&found, testfile));

				old = path.ptr[root];
				path.ptr[root] = '\0';
				setenv_and_check("HOMEDRIVE", path.ptr);
				path.ptr[root] = old;
				setenv_and_check("HOMEPATH", &path.ptr[root]);
				reset_global_search_path();

				cl_git_pass(git_futils_find_global_file(&found, testfile));
			}
		}
#endif

		(void)p_rmdir(*val);
	}

	git_buf_free(&path);
	git_buf_free(&found);
}


void test_core_env__1(void)
{
	git_buf path = GIT_BUF_INIT;

	cl_assert_equal_i(
		GIT_ENOTFOUND, git_futils_find_global_file(&path, "nonexistentfile"));

	cl_git_pass(cl_setenv("HOME", "doesnotexist"));
#ifdef GIT_WIN32
	cl_git_pass(cl_setenv("HOMEPATH", "doesnotexist"));
	cl_git_pass(cl_setenv("USERPROFILE", "doesnotexist"));
#endif
	reset_global_search_path();

	cl_assert_equal_i(
		GIT_ENOTFOUND, git_futils_find_global_file(&path, "nonexistentfile"));

	cl_git_pass(cl_setenv("HOME", NULL));
#ifdef GIT_WIN32
	cl_git_pass(cl_setenv("HOMEPATH", NULL));
	cl_git_pass(cl_setenv("USERPROFILE", NULL));
#endif
	reset_global_search_path();
	reset_system_search_path();

	cl_assert_equal_i(
		GIT_ENOTFOUND, git_futils_find_global_file(&path, "nonexistentfile"));

	cl_assert_equal_i(
		GIT_ENOTFOUND, git_futils_find_system_file(&path, "nonexistentfile"));

#ifdef GIT_WIN32
	cl_git_pass(cl_setenv("PROGRAMFILES", NULL));
	reset_system_search_path();

	cl_assert_equal_i(
		GIT_ENOTFOUND, git_futils_find_system_file(&path, "nonexistentfile"));
#endif

	git_buf_free(&path);
}

void test_core_env__2(void)
{
	git_buf path = GIT_BUF_INIT, found = GIT_BUF_INIT;
	char testfile[16], tidx = '0';
	char **val;
	const char *testname = "alternate";
	size_t testlen = strlen(testname);
	char out[GIT_PATH_MAX];

	strncpy(testfile, testname, sizeof(testfile));
	cl_assert_equal_s(testname, testfile);

	for (val = home_values; *val != NULL; val++) {

		/* if we can't make the directory, let's just assume
		 * we are on a filesystem that doesn't support the
		 * characters in question and skip this test...
		 */
		if (p_mkdir(*val, 0777) != 0 && errno != EEXIST) {
			*val = ""; /* mark as not created */
			continue;
		}

		cl_git_pass(git_path_prettify(&path, *val, NULL));

		/* vary testfile name in each directory so accidentally leaving
		 * an environment variable set from a previous iteration won't
		 * accidentally make this test pass...
		 */
		testfile[testlen] = tidx++;
		cl_git_pass(git_buf_joinpath(&path, path.ptr, testfile));
		cl_git_mkfile(path.ptr, "find me");
		git_buf_rtruncate_at_char(&path, '/');

		/* default should be NOTFOUND */
		cl_assert_equal_i(
			GIT_ENOTFOUND, git_futils_find_global_file(&found, testfile));

		/* set search path */
		cl_git_pass(git_libgit2_opts(
			GIT_OPT_SET_SEARCH_PATH, GIT_CONFIG_LEVEL_GLOBAL, path.ptr));

		cl_git_pass(git_libgit2_opts(
			GIT_OPT_GET_SEARCH_PATH, GIT_CONFIG_LEVEL_GLOBAL, out, sizeof(out)));
		cl_assert_equal_s(out, path.ptr);

		cl_git_pass(git_futils_find_global_file(&found, testfile));

		/* reset */
		cl_git_pass(git_libgit2_opts(
			GIT_OPT_SET_SEARCH_PATH, GIT_CONFIG_LEVEL_GLOBAL, NULL));
		cl_assert_equal_i(
			GIT_ENOTFOUND, git_futils_find_global_file(&found, testfile));

		/* try prepend behavior */
		cl_git_pass(git_buf_putc(&path, GIT_PATH_LIST_SEPARATOR));
		cl_git_pass(git_buf_puts(&path, "$PATH"));

		cl_git_pass(git_libgit2_opts(
			GIT_OPT_SET_SEARCH_PATH, GIT_CONFIG_LEVEL_GLOBAL, path.ptr));

		git_buf_rtruncate_at_char(&path, GIT_PATH_LIST_SEPARATOR);

		cl_git_pass(git_libgit2_opts(
			GIT_OPT_GET_SEARCH_PATH, GIT_CONFIG_LEVEL_GLOBAL, out, sizeof(out)));
		cl_assert(git__prefixcmp(out, path.ptr) == 0);

		cl_git_pass(git_futils_find_global_file(&found, testfile));

		/* reset */
		cl_git_pass(git_libgit2_opts(
			GIT_OPT_SET_SEARCH_PATH, GIT_CONFIG_LEVEL_GLOBAL, NULL));
		cl_assert_equal_i(
			GIT_ENOTFOUND, git_futils_find_global_file(&found, testfile));

		/* try append behavior */
		cl_git_pass(git_buf_join(
			&found, GIT_PATH_LIST_SEPARATOR, "$PATH", path.ptr));

		cl_git_pass(git_libgit2_opts(
			GIT_OPT_SET_SEARCH_PATH, GIT_CONFIG_LEVEL_GLOBAL, found.ptr));

		cl_git_pass(git_libgit2_opts(
			GIT_OPT_GET_SEARCH_PATH, GIT_CONFIG_LEVEL_GLOBAL, out, sizeof(out)));
		cl_assert(git__suffixcmp(out, path.ptr) == 0);

		cl_git_pass(git_futils_find_global_file(&found, testfile));

		/* reset */
		cl_git_pass(git_libgit2_opts(
			GIT_OPT_SET_SEARCH_PATH, GIT_CONFIG_LEVEL_GLOBAL, NULL));

		cl_git_pass(git_buf_joinpath(&path, path.ptr, testfile));
		(void)p_unlink(path.ptr);

		(void)p_rmdir(*val);
	}

	git_buf_free(&path);
	git_buf_free(&found);
}
