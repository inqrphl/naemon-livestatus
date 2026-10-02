#include <cppunit/extensions/HelperMacros.h>

class DoubleFormatTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE( DoubleFormatTest );
	CPPUNIT_TEST( testDoublesMatchSnprintf );
	CPPUNIT_TEST( testDoubleCacheReuse );
	CPPUNIT_TEST( testDoubleCacheOnSecondThread );
	CPPUNIT_TEST_SUITE_END();

	public:
	void setUp();
	void tearDown();
	void testDoublesMatchSnprintf();
	void testDoubleCacheReuse();
	void testDoubleCacheOnSecondThread();
};
