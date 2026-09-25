#include <cppunit/CompilerOutputter.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TextTestRunner.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <locale.h>
#include <pthread.h>
#include "NumberFormatTest.h"
#include "../src/NumberFormat.h"

CPPUNIT_TEST_SUITE_REGISTRATION( NumberFormatTest );

// The point of these formatters is that they are *byte exact* replacements for the
// snprintf() calls they replaced, so every test here compares against snprintf()
// itself rather than against a hand written expectation.

void
NumberFormatTest::setUp()
{
}

void
NumberFormatTest::tearDown()
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
NumberFormatTest::testIntegersMatchSnprintf()
{
	static const int64_t values[] = {
		0, 1, -1, 9, 10, -9, -10, 42, 99, 100, -99, -100, 127, 128, 255, 256,
		32767, 32768, 65535, 65536, 2147483647LL, -2147483648LL, 4294967295LL,
		4294967296LL, 999999999999LL, -999999999999LL,
		9223372036854775807LL, -9223372036854775807LL, INT64_MIN
	};
	char buf[24], want[64];

	for (unsigned i = 0; i < sizeof(values) / sizeof(values[0]); i++) {
		char *end = buf + sizeof(buf);
		char *p = formatIntegerBackward(end, values[i]);
		snprintf(want, sizeof(want), "%lld", (long long)values[i]);
		CPPUNIT_ASSERT_EQUAL(std::string(want), std::string(p, (size_t)(end - p)));
	}

	for (int i = 0; i < 200000; i++) {
		int64_t v = (int64_t)rng();
		char *end = buf + sizeof(buf);
		char *p = formatIntegerBackward(end, v);
		snprintf(want, sizeof(want), "%lld", (long long)v);
		CPPUNIT_ASSERT_EQUAL(std::string(want), std::string(p, (size_t)(end - p)));
	}
}

void
NumberFormatTest::testUnsignedMatchSnprintf()
{
	static const uint64_t values[] = { 0, 1, 9, 10, 255, 65535, 4294967295ULL,
	                                   9223372036854775807ULL, UINT64_MAX };
	char buf[24], want[64];

	for (unsigned i = 0; i < sizeof(values) / sizeof(values[0]); i++) {
		char *end = buf + sizeof(buf);
		char *p = formatUnsignedBackward(end, values[i]);
		snprintf(want, sizeof(want), "%llu", (unsigned long long)values[i]);
		CPPUNIT_ASSERT_EQUAL(std::string(want), std::string(p, (size_t)(end - p)));
	}

	for (int i = 0; i < 200000; i++) {
		uint64_t v = rng();
		char *end = buf + sizeof(buf);
		char *p = formatUnsignedBackward(end, v);
		snprintf(want, sizeof(want), "%llu", (unsigned long long)v);
		CPPUNIT_ASSERT_EQUAL(std::string(want), std::string(p, (size_t)(end - p)));
	}
}

void
NumberFormatTest::testTimeValMatchesSnprintf()
{
	char buf[40], want[64];
	time_t secs = 0;
	long usec = 0;

	for (int i = 0; i < 100000; i++) {
		secs = (time_t)(rng() >> 24);
		usec = (long)(rng() % 1000000);
		if (i == 0) { secs = 0; usec = 0; }
		if (i == 1) { secs = 1790275176; usec = 665651; }
		if (i == 2) { secs = 0; usec = 999999; }
		if (i == 3) { secs = 100; usec = 999990; }
		if (i == 4) { secs = 1; usec = 1; }
		int len = formatTimeVal(buf, secs, usec);
		snprintf(want, sizeof(want), "%lu.%06lu", (unsigned long)secs, (unsigned long)usec);
		CPPUNIT_ASSERT_EQUAL(std::string(want), std::string(buf, (size_t)len));
	}
}

void
NumberFormatTest::testDoublesMatchSnprintf()
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
		const char *got = formatDouble(values[i], &len);
		snprintf(want, sizeof(want), "%.10e", values[i]);
		CPPUNIT_ASSERT_EQUAL(std::string(want), std::string(got, (size_t)len));
	}

	/* random bit patterns: NaN payloads, infinities and denormals included */
	for (int i = 0; i < 200000; i++) {
		uint64_t bits = rng();
		double d;
		memcpy(&d, &bits, sizeof(d));
		int len;
		const char *got = formatDouble(d, &len);
		snprintf(want, sizeof(want), "%.10e", d);
		CPPUNIT_ASSERT_EQUAL(std::string(want), std::string(got, (size_t)len));
	}
}

void
NumberFormatTest::testDoubleCacheReuse()
{
	int len1 = 0, len2 = 0;
	const char *first = formatDouble(1234.5678, &len1);
	const char *second = formatDouble(1234.5678, &len2);

	// the second lookup must be served from the cache
	CPPUNIT_ASSERT(first == second);
	CPPUNIT_ASSERT_EQUAL(len1, len2);
	CPPUNIT_ASSERT_EQUAL(std::string("1.2345678000e+03"), std::string(second, (size_t)len2));

	// fill far more slots than the cache has, evicting the entry above
	char want[64];
	for (int i = 0; i < 5000; i++) {
		double d = (double)i * 1.5;
		int len;
		const char *got = formatDouble(d, &len);
		snprintf(want, sizeof(want), "%.10e", d);
		CPPUNIT_ASSERT_EQUAL(std::string(want), std::string(got, (size_t)len));
	}

	// and the refilled entry still has to be right
	int len3 = 0;
	const char *third = formatDouble(1234.5678, &len3);
	CPPUNIT_ASSERT_EQUAL(std::string("1.2345678000e+03"), std::string(third, (size_t)len3));
}

struct thread_args {
	int failures;
	long checks;
};

static void *numberformat_thread(void *p)
{
	struct thread_args *a = (struct thread_args *)p;
	for (int i = 0; i < 50000; i++) {
		uint64_t bits = rng();
		double d;
		memcpy(&d, &bits, sizeof(d));
		int len;
		const char *got = formatDouble(d, &len);
		char want[64];
		snprintf(want, sizeof(want), "%.10e", d);
		a->checks++;
		if (std::string(want) != std::string(got, (size_t)len)) a->failures++;
	}
	return NULL;
}

void
NumberFormatTest::testDoubleCacheOnSecondThread()
{
	// The cache is thread local; a second thread must get its own copy and its own correct answers, not the first thread's.
	struct thread_args args;
	args.failures = 0;
	args.checks = 0;

	// warm this thread's cache first
	int warm_len = 0;
	formatDouble(42.0, &warm_len);

	pthread_t t;
	CPPUNIT_ASSERT_EQUAL(0, pthread_create(&t, NULL, numberformat_thread, &args));
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
