#include <cppunit/extensions/HelperMacros.h>

class NumberFormatTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE( NumberFormatTest );
	CPPUNIT_TEST( testIntegersMatchSnprintf );
	CPPUNIT_TEST( testUnsignedMatchSnprintf );
	CPPUNIT_TEST( testTimeValMatchesSnprintf );
	CPPUNIT_TEST( testDoublesMatchSnprintf );
	CPPUNIT_TEST( testDoubleCacheReuse );
	CPPUNIT_TEST( testDoubleCacheOnSecondThread );
	CPPUNIT_TEST_SUITE_END();

	public:
	void setUp();
	void tearDown();
	void testIntegersMatchSnprintf();
	void testUnsignedMatchSnprintf();
	void testTimeValMatchesSnprintf();
	void testDoublesMatchSnprintf();
	void testDoubleCacheReuse();
	void testDoubleCacheOnSecondThread();
};
