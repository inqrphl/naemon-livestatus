#include <cppunit/CompilerOutputter.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TextTestRunner.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <locale.h>
#include <pthread.h>
#include "DoubleFormatTest.h"
#include "../src/DoubleFormat.h"

CPPUNIT_TEST_SUITE_REGISTRATION( DoubleFormatTest );

// The point of the cache is that it is a *byte exact* replacement for the
// snprintf() call it replaced, so every test here compares against snprintf()
// itself rather than against a hand written expectation.

void
DoubleFormatTest::setUp()
{
}

void
DoubleFormatTest::tearDown()
{
}

static uint64_t rng_state = 88172645463325252ULL;
static uint64_t rng()
{
	rng_state ^= rng_state << 13;
	rng_state ^= rng_state >> 7;
	rng_state ^= rng_state << 17;
	return rng_state;
}

void
DoubleFormatTest::testDoublesMatchSnprintf()
{
	static const double values[] = {
		0.0, -0.0, 1.0, -1.0, 0.1, 0.5, 30.0, 60.0, 5.0e-3, 1e308, -1e308, 1e-308,
		4.9406564584124654e-324, 2.2250738585072014e-308, 1.7976931348623157e308,
		3.14159265358979, -0.0000000001, 1e15, 1e16, 12345.6789, 180.47248292,
		994637.34889
	};
	char want[64];

	for (unsigned i = 0; i < sizeof(values) / sizeof(values[0]); i++) {
		int len;
		const char *got = formatDoubleDot10e(values[i], &len);
		snprintf(want, sizeof(want), "%.10e", values[i]);
		CPPUNIT_ASSERT_EQUAL(std::string(want), std::string(got, (size_t)len));
	}

	/* random bit patterns: NaN payloads, infinities and denormals included */
	for (int i = 0; i < 200000; i++) {
		uint64_t bits = rng();
		double d;
		memcpy(&d, &bits, sizeof(d));
		int len;
		const char *got = formatDoubleDot10e(d, &len);
		snprintf(want, sizeof(want), "%.10e", d);
		CPPUNIT_ASSERT_EQUAL(std::string(want), std::string(got, (size_t)len));
	}
}

void
DoubleFormatTest::testDoubleCacheReuse()
{
	int len1 = 0, len2 = 0;
	const char *first = formatDoubleDot10e(1234.5678, &len1);
	const char *second = formatDoubleDot10e(1234.5678, &len2);

	// the second lookup must be served from the cache
	CPPUNIT_ASSERT(first == second);
	CPPUNIT_ASSERT_EQUAL(len1, len2);
	CPPUNIT_ASSERT_EQUAL(std::string("1.2345678000e+03"), std::string(second, (size_t)len2));

	// fill far more slots than the cache has, evicting the entry above
	char want[64];
	for (int i = 0; i < 5000; i++) {
		double d = (double)i * 1.5;
		int len;
		const char *got = formatDoubleDot10e(d, &len);
		snprintf(want, sizeof(want), "%.10e", d);
		CPPUNIT_ASSERT_EQUAL(std::string(want), std::string(got, (size_t)len));
	}

	// and the refilled entry still has to be right
	int len3 = 0;
	const char *third = formatDoubleDot10e(1234.5678, &len3);
	CPPUNIT_ASSERT_EQUAL(std::string("1.2345678000e+03"), std::string(third, (size_t)len3));
}

struct thread_args {
	int failures;
	long checks;
};

static void *doubleformat_thread(void *p)
{
	struct thread_args *a = (struct thread_args *)p;
	for (int i = 0; i < 50000; i++) {
		uint64_t bits = rng();
		double d;
		memcpy(&d, &bits, sizeof(d));
		int len;
		const char *got = formatDoubleDot10e(d, &len);
		char want[64];
		snprintf(want, sizeof(want), "%.10e", d);
		a->checks++;
		if (std::string(want) != std::string(got, (size_t)len)) a->failures++;
	}
	return NULL;
}

void
DoubleFormatTest::testDoubleCacheOnSecondThread()
{
	// The cache is thread local; a second thread must get its own copy and its own correct answers, not the first thread's.
	struct thread_args args;
	args.failures = 0;
	args.checks = 0;

	// warm this thread's cache first
	int warm_len = 0;
	formatDoubleDot10e(42.0, &warm_len);

	pthread_t t;
	CPPUNIT_ASSERT_EQUAL(0, pthread_create(&t, NULL, doubleformat_thread, &args));
	pthread_join(t, NULL);

	CPPUNIT_ASSERT_EQUAL(0, args.failures);
	CPPUNIT_ASSERT_EQUAL((long)50000, args.checks);
}

int main(int argc, char * argv[])
{
	CppUnit::Test *suite = CppUnit::TestFactoryRegistry::getRegistry().makeTest();
	CppUnit::TextTestRunner runner;
	setlocale(LC_ALL, "");
	runner.addTest(suite);
	runner.setOutputter(new CppUnit::CompilerOutputter( &runner.result(), std::cerr));

	bool wasSuccessful = runner.run();

	return wasSuccessful ? 0 : 1;
}
