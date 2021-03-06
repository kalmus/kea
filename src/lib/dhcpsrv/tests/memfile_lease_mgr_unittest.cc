// Copyright (C) 2012-2015 Internet Systems Consortium, Inc. ("ISC")
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
// REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
// AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
// INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
// LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
// OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
// PERFORMANCE OF THIS SOFTWARE.

#include <config.h>

#include <asiolink/io_address.h>
#include <dhcp/duid.h>
#include <dhcpsrv/cfgmgr.h>
#include <dhcpsrv/lease_mgr.h>
#include <dhcpsrv/lease_mgr_factory.h>
#include <dhcpsrv/memfile_lease_mgr.h>
#include <dhcpsrv/tests/lease_file_io.h>
#include <dhcpsrv/tests/test_utils.h>
#include <dhcpsrv/tests/generic_lease_mgr_unittest.h>
#include <gtest/gtest.h>

#include <boost/bind.hpp>

#include <iostream>
#include <fstream>
#include <sstream>

using namespace std;
using namespace isc;
using namespace isc::asiolink;
using namespace isc::dhcp;
using namespace isc::dhcp::test;

namespace {

/// @brief Class derived from @c Memfile_LeaseMgr to test LFC timer.
///
/// This class provides a custom callback function which is invoked
/// when the timer for Lease File Cleanup goes off. It is used to
/// test that the timer is correctly installed.
class LFCMemfileLeaseMgr : public Memfile_LeaseMgr {
public:

    /// @brief Constructor.
    ///
    /// Sets the counter for callbacks to 0.
    LFCMemfileLeaseMgr(const ParameterMap& parameters)
        : Memfile_LeaseMgr(parameters), lfc_cnt_(0) {
    }

    /// @brief Returns the number of callback executions.
    int getLFCCount() {
        return (lfc_cnt_);
    }

protected:

    /// @brief Custom callback.
    ///
    /// This callback function increases the counter of callback executions.
    /// By examining the counter value a test may verify that the callback
    /// was triggered an expected number of times.
    virtual void lfcCallback() {
        ++lfc_cnt_;
    }

private:

    /// @brief Counter of callback function executions.
    int lfc_cnt_;

};

/// @brief Test fixture class for @c Memfile_LeaseMgr
class MemfileLeaseMgrTest : public GenericLeaseMgrTest {
public:

    /// @brief memfile lease mgr test constructor
    ///
    /// Creates memfile and stores it in lmptr_ pointer
    MemfileLeaseMgrTest() :
        io4_(getLeaseFilePath("leasefile4_0.csv")),
        io6_(getLeaseFilePath("leasefile6_0.csv")),
        io_service_(),
        fail_on_callback_(false) {

        // Make sure there are no dangling files after previous tests.
        io4_.removeFile();
        io6_.removeFile();
    }

    /// @brief Reopens the connection to the backend.
    ///
    /// This function is called by unit tests to force reconnection of the
    /// backend to check that the leases are stored and can be retrieved
    /// from the storage.
    ///
    /// @param u Universe (V4 or V6)
    virtual void reopen(Universe u) {
        LeaseMgrFactory::destroy();
        startBackend(u);
    }

    /// @brief destructor
    ///
    /// destroys lease manager backend.
    virtual ~MemfileLeaseMgrTest() {
        LeaseMgrFactory::destroy();
    }

    /// @brief Return path to the lease file used by unit tests.
    ///
    /// @param filename Name of the lease file appended to the path to the
    /// directory where test data is held.
    ///
    /// @return Full path to the lease file.
    static std::string getLeaseFilePath(const std::string& filename) {
        std::ostringstream s;
        s << TEST_DATA_BUILDDIR << "/" << filename;
        return (s.str());
    }

    /// @brief Returns the configuration string for the backend.
    ///
    /// This string configures the @c LeaseMgrFactory to create the memfile
    /// backend and use leasefile4_0.csv and leasefile6_0.csv files as
    /// storage for leases.
    ///
    /// @param no_universe Indicates whether universe parameter should be
    /// included (false), or not (true).
    ///
    /// @return Configuration string for @c LeaseMgrFactory.
    static std::string getConfigString(Universe u) {
        std::ostringstream s;
        s << "type=memfile " << (u == V4 ? "universe=4 " : "universe=6 ")
          << "name="
          << getLeaseFilePath(u == V4 ? "leasefile4_0.csv" : "leasefile6_0.csv");
        return (s.str());
    }

    /// @brief Creates instance of the backend.
    ///
    /// @param u Universe (v4 or V6).
    void startBackend(Universe u) {
        try {
            LeaseMgrFactory::create(getConfigString(u));
        } catch (...) {
            std::cerr << "*** ERROR: unable to create instance of the Memfile\n"
                " lease database backend.\n";
            throw;
        }
        lmptr_ = &(LeaseMgrFactory::instance());
    }

    void setTestTime(const uint32_t ms) {
        IntervalTimer::Callback cb =
            boost::bind(&MemfileLeaseMgrTest::testTimerCallback, this);
        test_timer_.reset(new IntervalTimer(*io_service_));
        test_timer_->setup(cb, ms, IntervalTimer::ONE_SHOT);
    }

    /// @brief Test timer callback function.
    void testTimerCallback() {
        io_service_->stop();
        if (fail_on_callback_) {
            FAIL() << "Test timeout reached";
        }
    }

    /// @brief Object providing access to v4 lease IO.
    LeaseFileIO io4_;

    /// @brief Object providing access to v6 lease IO.
    LeaseFileIO io6_;

    /// @brief Test timer for the test.
    boost::shared_ptr<IntervalTimer> test_timer_;

    /// @brief IO service object used for the timer tests.
    asiolink::IOServicePtr io_service_;

    /// @brief Indicates if the @c testTimerCallback should cause test failure.
    bool fail_on_callback_;

};

// This test checks if the LeaseMgr can be instantiated and that it
// parses parameters string properly.
TEST_F(MemfileLeaseMgrTest, constructor) {
    LeaseMgr::ParameterMap pmap;
    pmap["universe"] = "4";
    pmap["persist"] = "false";
    boost::scoped_ptr<Memfile_LeaseMgr> lease_mgr;

    EXPECT_NO_THROW(lease_mgr.reset(new Memfile_LeaseMgr(pmap)));

    pmap["lfc-interval"] = "10";
    pmap["persist"] = "true";
    pmap["name"] = getLeaseFilePath("leasefile4_1.csv");
    EXPECT_NO_THROW(lease_mgr.reset(new Memfile_LeaseMgr(pmap)));

    // Expecting that persist parameter is yes or no. Everything other than
    // that is wrong.
    pmap["persist"] = "bogus";
    pmap["name"] = getLeaseFilePath("leasefile4_1.csv");
    EXPECT_THROW(lease_mgr.reset(new Memfile_LeaseMgr(pmap)), isc::BadValue);

    // The lfc-interval must be an integer.
    pmap["lfc-interval"] = "bogus";
    EXPECT_THROW(lease_mgr.reset(new Memfile_LeaseMgr(pmap)), isc::BadValue);
}

// Checks if the getType() and getName() methods both return "memfile".
TEST_F(MemfileLeaseMgrTest, getTypeAndName) {
    startBackend(V4);
    EXPECT_EQ(std::string("memfile"), lmptr_->getType());
    EXPECT_EQ(std::string("memory"),  lmptr_->getName());
}

// Checks if the path to the lease files is initialized correctly.
TEST_F(MemfileLeaseMgrTest, getLeaseFilePath) {
    // Initialize IO objects, so as the test csv files get removed after the
    // test (when destructors are called).
    LeaseFileIO io4(getLeaseFilePath("leasefile4_1.csv"));
    LeaseFileIO io6(getLeaseFilePath("leasefile6_1.csv"));

    LeaseMgr::ParameterMap pmap;
    pmap["universe"] = "4";
    pmap["name"] = getLeaseFilePath("leasefile4_1.csv");
    boost::scoped_ptr<Memfile_LeaseMgr> lease_mgr(new Memfile_LeaseMgr(pmap));

    EXPECT_EQ(pmap["name"],
              lease_mgr->getLeaseFilePath(Memfile_LeaseMgr::V4));

    pmap["persist"] = "false";
    lease_mgr.reset(new Memfile_LeaseMgr(pmap));
    EXPECT_TRUE(lease_mgr->getLeaseFilePath(Memfile_LeaseMgr::V4).empty());
    EXPECT_TRUE(lease_mgr->getLeaseFilePath(Memfile_LeaseMgr::V6).empty());
}

// Check if the persitLeases correctly checks that leases should not be written
// to disk when disabled through configuration.
TEST_F(MemfileLeaseMgrTest, persistLeases) {
    // Initialize IO objects, so as the test csv files get removed after the
    // test (when destructors are called).
    LeaseFileIO io4(getLeaseFilePath("leasefile4_1.csv"));
    LeaseFileIO io6(getLeaseFilePath("leasefile6_1.csv"));

    LeaseMgr::ParameterMap pmap;
    pmap["universe"] = "4";
    // Specify the names of the lease files. Leases will be written.
    pmap["name"] = getLeaseFilePath("leasefile4_1.csv");
    boost::scoped_ptr<Memfile_LeaseMgr> lease_mgr(new Memfile_LeaseMgr(pmap));

    lease_mgr.reset(new Memfile_LeaseMgr(pmap));
    EXPECT_TRUE(lease_mgr->persistLeases(Memfile_LeaseMgr::V4));
    EXPECT_FALSE(lease_mgr->persistLeases(Memfile_LeaseMgr::V6));

    pmap["universe"] = "6";
    pmap["name"] = getLeaseFilePath("leasefile6_1.csv");
    lease_mgr.reset(new Memfile_LeaseMgr(pmap));
    EXPECT_FALSE(lease_mgr->persistLeases(Memfile_LeaseMgr::V4));
    EXPECT_TRUE(lease_mgr->persistLeases(Memfile_LeaseMgr::V6));

    // This should disable writes of leases to disk.
    pmap["persist"] = "false";
    lease_mgr.reset(new Memfile_LeaseMgr(pmap));
    EXPECT_FALSE(lease_mgr->persistLeases(Memfile_LeaseMgr::V4));
    EXPECT_FALSE(lease_mgr->persistLeases(Memfile_LeaseMgr::V6));
}

// Check if it is possible to schedule the timer to perform the Lease
// File Cleanup periodically.
TEST_F(MemfileLeaseMgrTest, lfcTimer) {
    LeaseMgr::ParameterMap pmap;
    pmap["type"] = "memfile";
    pmap["universe"] = "4";
    // Specify the names of the lease files. Leases will be written.
    pmap["name"] = getLeaseFilePath("leasefile4_0.csv");
    pmap["lfc-interval"] = "1";

    boost::scoped_ptr<LFCMemfileLeaseMgr>
        lease_mgr(new LFCMemfileLeaseMgr(pmap));

    // Check that the interval is correct.
    EXPECT_EQ(1, lease_mgr->getIOServiceExecInterval());

    io_service_ = lease_mgr->getIOService();

    // Run the test for at most 2.9 seconds.
    setTestTime(2900);

    // Run the IO service to execute timers.
    io_service_->run();

    // Within 2.9 we should record two LFC executions.
    EXPECT_EQ(2, lease_mgr->getLFCCount());
}


// This test checks if the LFC timer is disabled (doesn't trigger)
// cleanups when the lfc-interval is set to 0.
TEST_F(MemfileLeaseMgrTest, lfcTimerDisabled) {
    LeaseMgr::ParameterMap pmap;
    pmap["type"] = "memfile";
    pmap["universe"] = "4";
    pmap["name"] = getLeaseFilePath("leasefile4_0.csv");
    // Set the LFC interval to 0, which should effectively disable it.
    pmap["lfc-interval"] = "0";

    boost::scoped_ptr<LFCMemfileLeaseMgr>
        lease_mgr(new LFCMemfileLeaseMgr(pmap));

    EXPECT_EQ(0, lease_mgr->getIOServiceExecInterval());

    io_service_ = lease_mgr->getIOService();

    // Run the test for at most 1.9 seconds.
    setTestTime(1900);

    // Run the IO service to execute timers.
    io_service_->run();

    // There should be no LFC execution recorded.
    EXPECT_EQ(0, lease_mgr->getLFCCount());
}

// Test that the backend returns a correct value of the interval
// at which the IOService must be executed to run the handlers
// for the installed timers.
TEST_F(MemfileLeaseMgrTest, getIOServiceExecInterval) {
    LeaseMgr::ParameterMap pmap;
    pmap["type"] = "memfile";
    pmap["universe"] = "4";
    pmap["name"] = getLeaseFilePath("leasefile4_0.csv");

    // The lfc-interval is not set, so the returned value should be 0.
    boost::scoped_ptr<LFCMemfileLeaseMgr> lease_mgr(new LFCMemfileLeaseMgr(pmap));
    EXPECT_EQ(0, lease_mgr->getIOServiceExecInterval());

    // lfc-interval = 10
    pmap["lfc-interval"] = "10";
    lease_mgr.reset(new LFCMemfileLeaseMgr(pmap));
    EXPECT_EQ(10, lease_mgr->getIOServiceExecInterval());

    // lfc-interval = 20
    pmap["lfc-interval"] = "20";
    lease_mgr.reset(new LFCMemfileLeaseMgr(pmap));
    EXPECT_EQ(20, lease_mgr->getIOServiceExecInterval());

}

// Checks that adding/getting/deleting a Lease6 object works.
TEST_F(MemfileLeaseMgrTest, addGetDelete6) {
    startBackend(V6);
    testAddGetDelete6(true); // true - check T1,T2 values
    // memfile is able to preserve those values, but some other
    // backends can't do that.
}

/// @brief Basic Lease4 Checks
///
/// Checks that the addLease, getLease4 (by address) and deleteLease (with an
/// IPv4 address) works.
TEST_F(MemfileLeaseMgrTest, basicLease4) {
    startBackend(V4);
    testBasicLease4();
}

/// @todo Write more memfile tests

// Simple test about lease4 retrieval through client id method
TEST_F(MemfileLeaseMgrTest, getLease4ClientId) {
    startBackend(V4);
    testGetLease4ClientId();
}

// Checks that lease4 retrieval client id is null is working
TEST_F(MemfileLeaseMgrTest, getLease4NullClientId) {
    startBackend(V4);
    testGetLease4NullClientId();
}

// Checks lease4 retrieval through HWAddr
TEST_F(MemfileLeaseMgrTest, getLease4HWAddr1) {
    startBackend(V4);
    testGetLease4HWAddr1();
}

/// @brief Check GetLease4 methods - access by Hardware Address
///
/// Adds leases to the database and checks that they can be accessed via
/// a combination of DUID and IAID.
TEST_F(MemfileLeaseMgrTest, getLease4HWAddr2) {
    startBackend(V4);
    testGetLease4HWAddr2();
}

// Checks lease4 retrieval with clientId, HWAddr and subnet_id
TEST_F(MemfileLeaseMgrTest, getLease4ClientIdHWAddrSubnetId) {
    startBackend(V4);
    testGetLease4ClientIdHWAddrSubnetId();
}

/// @brief Basic Lease4 Checks
///
/// Checks that the addLease, getLease4(by address), getLease4(hwaddr,subnet_id),
/// updateLease4() and deleteLease (IPv4 address) can handle NULL client-id.
/// (client-id is optional and may not be present)
TEST_F(MemfileLeaseMgrTest, lease4NullClientId) {
    startBackend(V4);
    testLease4NullClientId();
}

/// @brief Check GetLease4 methods - access by Hardware Address & Subnet ID
///
/// Adds leases to the database and checks that they can be accessed via
/// a combination of hardware address and subnet ID
TEST_F(MemfileLeaseMgrTest, DISABLED_getLease4HwaddrSubnetId) {

    /// @todo: fails on memfile. It's probably a memfile bug.
    startBackend(V4);
    testGetLease4HWAddrSubnetId();
}

/// @brief Check GetLease4 methods - access by Client ID
///
/// Adds leases to the database and checks that they can be accessed via
/// the Client ID.
TEST_F(MemfileLeaseMgrTest, getLease4ClientId2) {
    startBackend(V4);
    testGetLease4ClientId2();
}

// @brief Get Lease4 by client ID
//
// Check that the system can cope with a client ID of any size.
TEST_F(MemfileLeaseMgrTest, getLease4ClientIdSize) {
    startBackend(V4);
    testGetLease4ClientIdSize();
}

/// @brief Check GetLease4 methods - access by Client ID & Subnet ID
///
/// Adds leases to the database and checks that they can be accessed via
/// a combination of client and subnet IDs.
TEST_F(MemfileLeaseMgrTest, getLease4ClientIdSubnetId) {
    startBackend(V4);
    testGetLease4ClientIdSubnetId();
}

/// @brief Basic Lease6 Checks
///
/// Checks that the addLease, getLease6 (by address) and deleteLease (with an
/// IPv6 address) works.
TEST_F(MemfileLeaseMgrTest, basicLease6) {
    startBackend(V6);
    testBasicLease6();
}


/// @brief Check GetLease6 methods - access by DUID/IAID
///
/// Adds leases to the database and checks that they can be accessed via
/// a combination of DUID and IAID.
/// @todo: test disabled, because Memfile_LeaseMgr::getLeases6(Lease::Type,
/// const DUID& duid, uint32_t iaid) const is not implemented yet.
TEST_F(MemfileLeaseMgrTest, getLeases6DuidIaid) {
    startBackend(V6);
    testGetLeases6DuidIaid();
}

// Check that the system can cope with a DUID of allowed size.

/// @todo: test disabled, because Memfile_LeaseMgr::getLeases6(Lease::Type,
/// const DUID& duid, uint32_t iaid) const is not implemented yet.
TEST_F(MemfileLeaseMgrTest, getLeases6DuidSize) {
    startBackend(V6);
    testGetLeases6DuidSize();
}

/// @brief Check that getLease6 methods discriminate by lease type.
///
/// Adds six leases, two per lease type all with the same duid and iad but
/// with alternating subnet_ids.
/// It then verifies that all of getLeases6() method variants correctly
/// discriminate between the leases based on lease type alone.
/// @todo: Disabled, because type parameter in Memfile_LeaseMgr::getLease6
/// (Lease::Type, const isc::asiolink::IOAddress& addr) const is not used.
TEST_F(MemfileLeaseMgrTest, lease6LeaseTypeCheck) {
    startBackend(V6);
    testLease6LeaseTypeCheck();
}

/// @brief Check GetLease6 methods - access by DUID/IAID/SubnetID
///
/// Adds leases to the database and checks that they can be accessed via
/// a combination of DIUID and IAID.
TEST_F(MemfileLeaseMgrTest, getLease6DuidIaidSubnetId) {
    startBackend(V6);
    testGetLease6DuidIaidSubnetId();
}

/// Checks that getLease6(type, duid, iaid, subnet-id) works with different
/// DUID sizes
TEST_F(MemfileLeaseMgrTest, getLease6DuidIaidSubnetIdSize) {
    startBackend(V6);
    testGetLease6DuidIaidSubnetIdSize();
}

/// @brief Lease4 update tests
///
/// Checks that we are able to update a lease in the database.
/// @todo: Disabled, because memfile does not throw when lease is updated.
/// We should reconsider if lease{4,6} structures should have a limit
/// implemented in them.
TEST_F(MemfileLeaseMgrTest, DISABLED_updateLease4) {
    startBackend(V4);
    testUpdateLease4();
}

/// @brief Lease6 update tests
///
/// Checks that we are able to update a lease in the database.
/// @todo: Disabled, because memfile does not throw when lease is updated.
/// We should reconsider if lease{4,6} structures should have a limit
/// implemented in them.
TEST_F(MemfileLeaseMgrTest, DISABLED_updateLease6) {
    startBackend(V6);
    testUpdateLease6();
}

/// @brief DHCPv4 Lease recreation tests
///
/// Checks that the lease can be created, deleted and recreated with
/// different parameters. It also checks that the re-created lease is
/// correctly stored in the lease database.
TEST_F(MemfileLeaseMgrTest, testRecreateLease4) {
    startBackend(V4);
    testRecreateLease4();
}

/// @brief DHCPv6 Lease recreation tests
///
/// Checks that the lease can be created, deleted and recreated with
/// different parameters. It also checks that the re-created lease is
/// correctly stored in the lease database.
TEST_F(MemfileLeaseMgrTest, testRecreateLease6) {
    startBackend(V6);
    testRecreateLease6();
}

// The following tests are not applicable for memfile. When adding
// new tests to the list here, make sure to provide brief explanation
// why they are not applicable:
//
// testGetLease4HWAddrSubnetIdSize() - memfile just keeps Lease structure
//     and does not do any checks of HWAddr content

/// @brief Checks that null DUID is not allowed.
/// Test is disabled as Memfile does not currently defend against a null DUID.
TEST_F(MemfileLeaseMgrTest, DISABLED_nullDuid) {
    // Create leases, although we need only one.
    vector<Lease6Ptr> leases = createLeases6();

    leases[1]->duid_.reset();
    ASSERT_THROW(lmptr_->addLease(leases[1]), DbOperationError);
}

/// @brief Tests whether memfile can store and retrieve hardware addresses
TEST_F(MemfileLeaseMgrTest, testLease6Mac) {
    startBackend(V6);
    testLease6MAC();
}

/// @brief Tests whether memfile is able to work with old CSV file (without mac)
///
/// Ticket #3555 introduced MAC address support in Lease6. Instead of developing
/// an upgrade script, the code is written in a way that allows reading old CSV
/// (i.e. format that was used in Kea 0.9), hence no upgrade is necessary.
TEST_F(MemfileLeaseMgrTest, testUpgrade0_9_0_to_0_9_1) {

    // Let's write a CSV file without hwaddr column. Sorry about the long
    // lines, but nobody was around to whine about 80 columns limit when CSV
    // format was invented :).
    string csv_nohwaddr =
        "address,duid,valid_lifetime,expire,subnet_id,pref_lifetime,lease_type,iaid,prefix_len,fqdn_fwd,fqdn_rev,hostname\n"
        "2001:db8::1,42:42:42:42:42:42:42:42,3677,127133,73,3600,1,42,0,0,1,myhost.example.com.\n"
        "2001:db8::2,3a:3a:3a:3a:3a:3a:3a:3a,5412,239979,73,1800,2,89,7,0,0,myhost.example.com.\n"
        "2001:db8::3,1f:20:21:22:23:24:25:26,7000,241567,37,7200,0,4294967294,28,1,0,myhost.example.com.\n";

    ofstream csv(getLeaseFilePath("leasefile6_0.csv").c_str(), ios::out | ios::trunc);
    ASSERT_TRUE(csv.is_open());
    csv << csv_nohwaddr;
    csv.close();

    startBackend(V6);

    // None of the leases should have any hardware addresses assigned.
    Lease6Ptr stored1 = lmptr_->getLease6(leasetype6_[1], ioaddress6_[1]);
    ASSERT_TRUE(stored1);
    EXPECT_FALSE(stored1->hwaddr_);

    Lease6Ptr stored2 = lmptr_->getLease6(leasetype6_[2], ioaddress6_[2]);
    ASSERT_TRUE(stored2);
    EXPECT_FALSE(stored2->hwaddr_);

    Lease6Ptr stored3 = lmptr_->getLease6(leasetype6_[3], ioaddress6_[3]);
    ASSERT_TRUE(stored3);
    EXPECT_FALSE(stored3->hwaddr_);
}

// Check that memfile reports version correctly.
TEST_F(MemfileLeaseMgrTest, versionCheck) {

    // Check that V4 backend reports versions correctly.
    startBackend(V4);
    testVersion(Memfile_LeaseMgr::MAJOR_VERSION,
                Memfile_LeaseMgr::MINOR_VERSION);
    LeaseMgrFactory::destroy();

    // Check that V6 backends reports them ok, too.
    startBackend(V6);
    testVersion(Memfile_LeaseMgr::MAJOR_VERSION,
                Memfile_LeaseMgr::MINOR_VERSION);
    LeaseMgrFactory::destroy();
}

// This test checks that the backend reads DHCPv4 lease data from multiple
// files.
TEST_F(MemfileLeaseMgrTest, load4MultipleLeaseFiles) {
    LeaseFileIO io2("leasefile4_0.csv.2");
    io2.writeFile("address,hwaddr,client_id,valid_lifetime,expire,subnet_id,"
                  "fqdn_fwd,fqdn_rev,hostname\n"
                  "192.0.2.2,02:02:02:02:02:02,,200,200,8,1,1,,\n"
                  "192.0.2.11,bb:bb:bb:bb:bb:bb,,200,200,8,1,1,,\n");

    LeaseFileIO io1("leasefile4_0.csv.1");
    io1.writeFile("address,hwaddr,client_id,valid_lifetime,expire,subnet_id,"
                  "fqdn_fwd,fqdn_rev,hostname\n"
                  "192.0.2.1,01:01:01:01:01:01,,200,200,8,1,1,,\n"
                  "192.0.2.11,bb:bb:bb:bb:bb:bb,,200,400,8,1,1,,\n"
                  "192.0.2.12,cc:cc:cc:cc:cc:cc,,200,200,8,1,1,,\n");

    LeaseFileIO io("leasefile4_0.csv");
    io.writeFile("address,hwaddr,client_id,valid_lifetime,expire,subnet_id,"
                 "fqdn_fwd,fqdn_rev,hostname\n"
                 "192.0.2.10,0a:0a:0a:0a:0a:0a,,200,200,8,1,1,,\n"
                 "192.0.2.12,cc:cc:cc:cc:cc:cc,,200,400,8,1,1,,\n");

    startBackend(V4);

    // This lease only exists in the second file and the cltt should
    // be 0.
    Lease4Ptr lease = lmptr_->getLease4(IOAddress("192.0.2.1"));
    ASSERT_TRUE(lease);
    EXPECT_EQ(0, lease->cltt_);

    // This lease only exists in the first file and the cltt should
    // be 0.
    lease = lmptr_->getLease4(IOAddress("192.0.2.2"));
    ASSERT_TRUE(lease);
    EXPECT_EQ(0, lease->cltt_);

    // This lease only exists in the third file and the cltt should
    // be 0.
    lease = lmptr_->getLease4(IOAddress("192.0.2.10"));
    ASSERT_TRUE(lease);
    EXPECT_EQ(0, lease->cltt_);

    // This lease exists in the first and second file and the cltt
    // should be calculated using the expiration time and the
    // valid lifetime from the second file.
    lease = lmptr_->getLease4(IOAddress("192.0.2.11"));
    ASSERT_TRUE(lease);
    EXPECT_EQ(200, lease->cltt_);

    // Thsi lease exists in the second and third file and the cltt
    // should be calculated using the expiration time and the
    // valid lifetime from the third file.
    lease = lmptr_->getLease4(IOAddress("192.0.2.12"));
    ASSERT_TRUE(lease);
    EXPECT_EQ(200, lease->cltt_);
}

// This test checks that the lease database backend loads the file with
// the .completed postfix instead of files with postfixes .1 and .2 if
// the file with .completed postfix exists.
TEST_F(MemfileLeaseMgrTest, load4CompletedFile) {
    LeaseFileIO io2("leasefile4_0.csv.2");
    io2.writeFile("address,hwaddr,client_id,valid_lifetime,expire,subnet_id,"
                  "fqdn_fwd,fqdn_rev,hostname\n"
                  "192.0.2.2,02:02:02:02:02:02,,200,200,8,1,1,,\n"
                  "192.0.2.11,bb:bb:bb:bb:bb:bb,,200,200,8,1,1,,\n");

    LeaseFileIO io1("leasefile4_0.csv.1");
    io1.writeFile("address,hwaddr,client_id,valid_lifetime,expire,subnet_id,"
                  "fqdn_fwd,fqdn_rev,hostname\n"
                  "192.0.2.1,01:01:01:01:01:01,,200,200,8,1,1,,\n"
                  "192.0.2.11,bb:bb:bb:bb:bb:bb,,200,400,8,1,1,,\n"
                  "192.0.2.12,cc:cc:cc:cc:cc:cc,,200,200,8,1,1,,\n");

    LeaseFileIO io("leasefile4_0.csv");
    io.writeFile("address,hwaddr,client_id,valid_lifetime,expire,subnet_id,"
                 "fqdn_fwd,fqdn_rev,hostname\n"
                 "192.0.2.10,0a:0a:0a:0a:0a:0a,,200,200,8,1,1,,\n"
                 "192.0.2.12,cc:cc:cc:cc:cc:cc,,200,400,8,1,1,,\n");

    LeaseFileIO ioc("leasefile4_0.csv.completed");
    ioc.writeFile("address,hwaddr,client_id,valid_lifetime,expire,subnet_id,"
                  "fqdn_fwd,fqdn_rev,hostname\n"
                  "192.0.2.13,ff:ff:ff:ff:ff:ff,,200,200,8,1,1,,\n");

    startBackend(V4);

    // We expect that this file only holds leases that belong to the
    // lease file or to the file with .completed postfix.
    Lease4Ptr lease = lmptr_->getLease4(IOAddress("192.0.2.10"));
    ASSERT_TRUE(lease);
    EXPECT_EQ(0, lease->cltt_);

    lease = lmptr_->getLease4(IOAddress("192.0.2.12"));
    ASSERT_TRUE(lease);
    EXPECT_EQ(200, lease->cltt_);

    // This lease is in the .completed file.
    lease = lmptr_->getLease4(IOAddress("192.0.2.13"));
    ASSERT_TRUE(lease);
    EXPECT_EQ(0, lease->cltt_);

    // Leases from the .1 and .2 files should not be loaded.
    EXPECT_FALSE(lmptr_->getLease4(IOAddress("192.0.2.11")));
    EXPECT_FALSE(lmptr_->getLease4(IOAddress("192.0.2.1")));
}

// This test checks that the backend reads DHCPv6 lease data from multiple
// files.
TEST_F(MemfileLeaseMgrTest, load6MultipleLeaseFiles) {
    LeaseFileIO io2("leasefile6_0.csv.2");
    io2.writeFile("address,duid,valid_lifetime,expire,subnet_id,pref_lifetime,"
                  "lease_type,iaid,prefix_len,fqdn_fwd,fqdn_rev,hostname,hwaddr\n"
                  "2001:db8:1::1,01:01:01:01:01:01:01:01:01:01:01:01:01,"
                  "200,200,8,100,0,7,0,1,1,,\n"
                  "2001:db8:1::2,02:02:02:02:02:02:02:02:02:02:02:02:02,"
                  "200,200,8,100,0,7,0,1,1,,\n");

    LeaseFileIO io1("leasefile6_0.csv.1");
    io1.writeFile("address,duid,valid_lifetime,expire,subnet_id,pref_lifetime,"
                  "lease_type,iaid,prefix_len,fqdn_fwd,fqdn_rev,hostname,hwaddr\n"
                  "2001:db8:1::3,03:03:03:03:03:03:03:03:03:03:03:03:03,"
                  "200,200,8,100,0,7,0,1,1,,\n"
                  "2001:db8:1::2,02:02:02:02:02:02:02:02:02:02:02:02:02,"
                  "300,800,8,100,0,7,0,1,1,,\n"
                  "2001:db8:1::4,04:04:04:04:04:04:04:04:04:04:04:04:04,"
                  "200,200,8,100,0,7,0,1,1,,\n");

    LeaseFileIO io("leasefile6_0.csv");
    io.writeFile("address,duid,valid_lifetime,expire,subnet_id,pref_lifetime,"
                 "lease_type,iaid,prefix_len,fqdn_fwd,fqdn_rev,hostname,hwaddr\n"
                 "2001:db8:1::4,04:04:04:04:04:04:04:04:04:04:04:04:04,"
                 "400,1000,8,100,0,7,0,1,1,,\n"
                 "2001:db8:1::5,05:05:05:05:05:05:05:05:05:05:05:05:05,"
                 "200,200,8,100,0,7,0,1,1,,\n");

    startBackend(V6);

    // This lease only exists in the first file and the cltt should be 0.
    Lease6Ptr lease = lmptr_->getLease6(Lease::TYPE_NA,
                                        IOAddress("2001:db8:1::1"));
    ASSERT_TRUE(lease);
    EXPECT_EQ(0, lease->cltt_);

    // This lease exists in the first and second file and the cltt should
    // be calculated using the expiration time and the valid lifetime
    // from the second file.
    lease = lmptr_->getLease6(Lease::TYPE_NA, IOAddress("2001:db8:1::2"));
    ASSERT_TRUE(lease);
    EXPECT_EQ(500, lease->cltt_);

    // This lease only exists in the second file and the cltt should be 0.
    lease = lmptr_->getLease6(Lease::TYPE_NA, IOAddress("2001:db8:1::3"));
    ASSERT_TRUE(lease);
    EXPECT_EQ(0, lease->cltt_);

    // This lease exists in the second and third file and the cltt should
    // be calculated using the expiration time and the valid lifetime
    // from the third file.
    lease = lmptr_->getLease6(Lease::TYPE_NA, IOAddress("2001:db8:1::4"));
    ASSERT_TRUE(lease);
    EXPECT_EQ(600, lease->cltt_);

    // This lease only exists in the third file and the cltt should be 0.
    lease = lmptr_->getLease6(Lease::TYPE_NA, IOAddress("2001:db8:1::5"));
    ASSERT_TRUE(lease);
    EXPECT_EQ(0, lease->cltt_);
}

// This test checks that the backend reads DHCPv6 lease data from the
// leasefile without the postfix and the file with a .1 postfix when
// the file with the .2 postfix is missing.
TEST_F(MemfileLeaseMgrTest, load6MultipleNoSecondFile) {
    LeaseFileIO io1("leasefile6_0.csv.1");
    io1.writeFile("address,duid,valid_lifetime,expire,subnet_id,pref_lifetime,"
                  "lease_type,iaid,prefix_len,fqdn_fwd,fqdn_rev,hostname,hwaddr\n"
                  "2001:db8:1::3,03:03:03:03:03:03:03:03:03:03:03:03:03,"
                  "200,200,8,100,0,7,0,1,1,,\n"
                  "2001:db8:1::2,02:02:02:02:02:02:02:02:02:02:02:02:02,"
                  "300,800,8,100,0,7,0,1,1,,\n"
                  "2001:db8:1::4,04:04:04:04:04:04:04:04:04:04:04:04:04,"
                  "200,200,8,100,0,7,0,1,1,,\n");

    LeaseFileIO io("leasefile6_0.csv");
    io.writeFile("address,duid,valid_lifetime,expire,subnet_id,pref_lifetime,"
                 "lease_type,iaid,prefix_len,fqdn_fwd,fqdn_rev,hostname,hwaddr\n"
                 "2001:db8:1::4,04:04:04:04:04:04:04:04:04:04:04:04:04,"
                 "400,1000,8,100,0,7,0,1,1,,\n"
                 "2001:db8:1::5,05:05:05:05:05:05:05:05:05:05:05:05:05,"
                 "200,200,8,100,0,7,0,1,1,,\n");

    startBackend(V6);

    // Check that leases from the leasefile6_0 and leasefile6_0.1 have
    // been loaded.
    Lease6Ptr lease = lmptr_->getLease6(Lease::TYPE_NA, IOAddress("2001:db8:1::2"));
    ASSERT_TRUE(lease);
    EXPECT_EQ(500, lease->cltt_);

    lease = lmptr_->getLease6(Lease::TYPE_NA, IOAddress("2001:db8:1::3"));
    ASSERT_TRUE(lease);
    EXPECT_EQ(0, lease->cltt_);

    lease = lmptr_->getLease6(Lease::TYPE_NA, IOAddress("2001:db8:1::4"));
    ASSERT_TRUE(lease);
    EXPECT_EQ(600, lease->cltt_);

    lease = lmptr_->getLease6(Lease::TYPE_NA, IOAddress("2001:db8:1::5"));
    ASSERT_TRUE(lease);
    EXPECT_EQ(0, lease->cltt_);

    // Make sure that a lease which is not in those files is not loaded.
    EXPECT_FALSE(lmptr_->getLease6(Lease::TYPE_NA, IOAddress("2001:db8:1::1")));
}

// This test checks that the backend reads DHCPv6 lease data from the
// leasefile without the postfix and the file with a .2 postfix when
// the file with the .1 postfix is missing.
TEST_F(MemfileLeaseMgrTest, load6MultipleNoFirstFile) {
    LeaseFileIO io2("leasefile6_0.csv.2");
    io2.writeFile("address,duid,valid_lifetime,expire,subnet_id,pref_lifetime,"
                  "lease_type,iaid,prefix_len,fqdn_fwd,fqdn_rev,hostname,hwaddr\n"
                  "2001:db8:1::1,01:01:01:01:01:01:01:01:01:01:01:01:01,"
                  "200,200,8,100,0,7,0,1,1,,\n"
                  "2001:db8:1::2,02:02:02:02:02:02:02:02:02:02:02:02:02,"
                  "200,200,8,100,0,7,0,1,1,,\n");

    LeaseFileIO io("leasefile6_0.csv");
    io.writeFile("address,duid,valid_lifetime,expire,subnet_id,pref_lifetime,"
                 "lease_type,iaid,prefix_len,fqdn_fwd,fqdn_rev,hostname,hwaddr\n"
                 "2001:db8:1::4,04:04:04:04:04:04:04:04:04:04:04:04:04,"
                 "400,1000,8,100,0,7,0,1,1,,\n"
                 "2001:db8:1::5,05:05:05:05:05:05:05:05:05:05:05:05:05,"
                 "200,200,8,100,0,7,0,1,1,,\n");

    startBackend(V6);

    // Verify that leases which belong to the leasefile6_0.csv and
    // leasefile6_0.2 are loaded.
    Lease6Ptr lease = lmptr_->getLease6(Lease::TYPE_NA,
                                        IOAddress("2001:db8:1::1"));
    ASSERT_TRUE(lease);
    EXPECT_EQ(0, lease->cltt_);

    lease = lmptr_->getLease6(Lease::TYPE_NA, IOAddress("2001:db8:1::2"));
    ASSERT_TRUE(lease);
    EXPECT_EQ(0, lease->cltt_);

    lease = lmptr_->getLease6(Lease::TYPE_NA, IOAddress("2001:db8:1::4"));
    ASSERT_TRUE(lease);
    EXPECT_EQ(600, lease->cltt_);

    lease = lmptr_->getLease6(Lease::TYPE_NA, IOAddress("2001:db8:1::5"));
    ASSERT_TRUE(lease);
    EXPECT_EQ(0, lease->cltt_);

    // A lease which doesn't belong to these files should not be loaded.
    EXPECT_FALSE(lmptr_->getLease6(Lease::TYPE_NA, IOAddress("2001:db8:1::3")));
}


// This test checks that the lease database backend loads the file with
// the .completed postfix instead of files with postfixes .1 and .2 if
// the file with .completed postfix exists.
TEST_F(MemfileLeaseMgrTest, load6CompletedFile) {
    LeaseFileIO io2("leasefile6_0.csv.2");
    io2.writeFile("address,duid,valid_lifetime,expire,subnet_id,pref_lifetime,"
                  "lease_type,iaid,prefix_len,fqdn_fwd,fqdn_rev,hostname,hwaddr\n"
                  "2001:db8:1::1,01:01:01:01:01:01:01:01:01:01:01:01:01,"
                  "200,200,8,100,0,7,0,1,1,,\n"
                  "2001:db8:1::2,02:02:02:02:02:02:02:02:02:02:02:02:02,"
                  "200,200,8,100,0,7,0,1,1,,\n");

    LeaseFileIO io1("leasefile6_0.csv.1");
    io1.writeFile("address,duid,valid_lifetime,expire,subnet_id,pref_lifetime,"
                  "lease_type,iaid,prefix_len,fqdn_fwd,fqdn_rev,hostname,hwaddr\n"
                  "2001:db8:1::3,03:03:03:03:03:03:03:03:03:03:03:03:03,"
                  "200,200,8,100,0,7,0,1,1,,\n"
                  "2001:db8:1::2,02:02:02:02:02:02:02:02:02:02:02:02:02,"
                  "300,800,8,100,0,7,0,1,1,,\n"
                  "2001:db8:1::4,04:04:04:04:04:04:04:04:04:04:04:04:04,"
                  "200,200,8,100,0,7,0,1,1,,\n");

    LeaseFileIO io("leasefile6_0.csv");
    io.writeFile("address,duid,valid_lifetime,expire,subnet_id,pref_lifetime,"
                 "lease_type,iaid,prefix_len,fqdn_fwd,fqdn_rev,hostname,hwaddr\n"
                 "2001:db8:1::4,04:04:04:04:04:04:04:04:04:04:04:04:04,"
                 "400,1000,8,100,0,7,0,1,1,,\n"
                 "2001:db8:1::5,05:05:05:05:05:05:05:05:05:05:05:05:05,"
                 "200,200,8,100,0,7,0,1,1,,\n");

    LeaseFileIO ioc("leasefile6_0.csv.completed");
    ioc.writeFile("address,duid,valid_lifetime,expire,subnet_id,pref_lifetime,"
                  "lease_type,iaid,prefix_len,fqdn_fwd,fqdn_rev,hostname,hwaddr\n"
                  "2001:db8:1::125,ff:ff:ff:ff:ff:ff:ff:ff:ff:ff:ff:ff:ff,"
                  "400,1000,8,100,0,7,0,1,1,,\n");

    startBackend(V6);

    // We expect that this file only holds leases that belong to the
    // lease file or to the file with .completed postfix.
    Lease6Ptr lease = lmptr_->getLease6(Lease::TYPE_NA, IOAddress("2001:db8:1::4"));
    ASSERT_TRUE(lease);
    EXPECT_EQ(600, lease->cltt_);

    lease = lmptr_->getLease6(Lease::TYPE_NA, IOAddress("2001:db8:1::5"));
    ASSERT_TRUE(lease);
    EXPECT_EQ(0, lease->cltt_);

    lease = lmptr_->getLease6(Lease::TYPE_NA, IOAddress("2001:db8:1::125"));
    ASSERT_TRUE(lease);
    EXPECT_EQ(600, lease->cltt_);

    // Leases from the .1 and .2 files should not be loaded.
    EXPECT_FALSE(lmptr_->getLease6(Lease::TYPE_NA, IOAddress("2001:db8:1::1")));
    EXPECT_FALSE(lmptr_->getLease6(Lease::TYPE_NA, IOAddress("2001:db8:1::2")));
    EXPECT_FALSE(lmptr_->getLease6(Lease::TYPE_NA, IOAddress("2001:db8:1::3")));
}

}; // end of anonymous namespace
