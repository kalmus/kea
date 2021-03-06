// Copyright (C) 2014-2015 Internet Systems Consortium, Inc. ("ISC")
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
#include <cc/data.h>
#include <dhcp/dhcp4.h>
#include <dhcp/tests/iface_mgr_test_config.h>
#include <dhcpsrv/cfgmgr.h>
#include <dhcpsrv/host.h>
#include <dhcpsrv/host_mgr.h>
#include <dhcpsrv/subnet_id.h>
#include <dhcp4/tests/dhcp4_test_utils.h>
#include <dhcp4/tests/dhcp4_client.h>
#include <boost/shared_ptr.hpp>

using namespace isc;
using namespace isc::asiolink;
using namespace isc::data;
using namespace isc::dhcp;
using namespace isc::dhcp::test;

namespace {

/// @brief Set of JSON configurations used throughout the DORA tests.
///
/// - Configuration 0:
///   - Used for testing direct traffic
///   - 1 subnet: 10.0.0.0/24
///   - 1 pool: 10.0.0.10-10.0.0.100
///   - Router option present: 10.0.0.200 and 10.0.0.201
///   - Domain Name Server option present: 10.0.0.202, 10.0.0.203.
///   - Log Servers option present: 192.0.2.200 and 192.0.2.201
///   - Quotes Servers option present: 192.0.2.202, 192.0.2.203.
///
/// - Configuration 1:
///   - Use for testing relayed messages
///   - 1 subnet: 192.0.2.0/24
///   - Router option present: 192.0.2.200 and 192.0.2.201
///   - Domain Name Server option present: 192.0.2.202, 192.0.2.203.
///   - Log Servers option present: 192.0.2.200 and 192.0.2.201
///   - Quotes Servers option present: 192.0.2.202, 192.0.2.203.
///
/// - Configuration 2:
///   - Use for testing simple scenarios with host reservations
///   - 1 subnet: 10.0.0.0/24
///   - One reservation for the client using MAC address:
///     aa:bb:cc:dd:ee:ff, reserved address 10.0.0.7
const char* DORA_CONFIGS[] = {
// Configuration 0
    "{ \"interfaces\": [ \"*\" ],"
        "\"valid-lifetime\": 600,"
        "\"subnet4\": [ { "
        "    \"subnet\": \"10.0.0.0/24\", "
        "    \"id\": 1,"
        "    \"pools\": [ { \"pool\": \"10.0.0.10-10.0.0.100\" } ],"
        "    \"option-data\": [ {"
        "        \"name\": \"routers\","
        "        \"code\": 3,"
        "        \"data\": \"10.0.0.200,10.0.0.201\","
        "        \"csv-format\": true,"
        "        \"space\": \"dhcp4\""
        "    },"
        "    {"
        "        \"name\": \"domain-name-servers\","
        "        \"code\": 6,"
        "        \"data\": \"10.0.0.202,10.0.0.203\","
        "        \"csv-format\": true,"
        "        \"space\": \"dhcp4\""
        "    },"
        "    {"
        "        \"name\": \"log-servers\","
        "        \"code\": 7,"
        "        \"data\": \"10.0.0.200,10.0.0.201\","
        "        \"csv-format\": true,"
        "        \"space\": \"dhcp4\""
        "    },"
        "    {"
        "        \"name\": \"cookie-servers\","
        "        \"code\": 8,"
        "        \"data\": \"10.0.0.202,10.0.0.203\","
        "        \"csv-format\": true,"
        "        \"space\": \"dhcp4\""
        "    } ]"
        " } ]"
    "}",

// Configuration 1
    "{ \"interfaces\": [ \"*\" ],"
        "\"valid-lifetime\": 600,"
        "\"subnet4\": [ { "
        "    \"subnet\": \"192.0.2.0/24\", "
        "    \"option-data\": [ {"
        "        \"name\": \"routers\","
        "        \"code\": 3,"
        "        \"data\": \"192.0.2.200,192.0.2.201\","
        "        \"csv-format\": true,"
        "        \"space\": \"dhcp4\""
        "    },"
        "    {"
        "        \"name\": \"domain-name-servers\","
        "        \"code\": 6,"
        "        \"data\": \"192.0.2.202,192.0.2.203\","
        "        \"csv-format\": true,"
        "        \"space\": \"dhcp4\""
        "    },"
        "    {"
        "        \"name\": \"log-servers\","
        "        \"code\": 7,"
        "        \"data\": \"10.0.0.200,10.0.0.201\","
        "        \"csv-format\": true,"
        "        \"space\": \"dhcp4\""
        "    },"
        "    {"
        "        \"name\": \"cookie-servers\","
        "        \"code\": 8,"
        "        \"data\": \"10.0.0.202,10.0.0.203\","
        "        \"csv-format\": true,"
        "        \"space\": \"dhcp4\""
        "    } ]"
        " } ]"
    "}",

// Configuration 2
    "{ \"interfaces\": [ \"*\" ],"
        "\"valid-lifetime\": 600,"
        "\"subnet4\": [ { "
        "    \"subnet\": \"10.0.0.0/24\", "
        "    \"pools\": [ { \"pool\": \"10.0.0.10-10.0.0.100\" } ],"
        "    \"reservations\": [ "
        "       {"
        "         \"hw-address\": \"aa:bb:cc:dd:ee:ff\","
        "         \"ip-address\": \"10.0.0.7\""
        "       }"
        "    ]"
        "} ]"
    "}"
};

/// @brief Test fixture class for testing 4-way (DORA) exchanges.
///
/// @todo Currently there is a limit number of test cases covered here.
/// In the future it is planned that the tests from the
/// dhcp4_srv_unittest.cc will be migrated here and will use the
/// @c Dhcp4Client class.
class DORATest : public Dhcpv4SrvTest {
public:

    /// @brief Constructor.
    ///
    /// Sets up fake interfaces.
    DORATest()
        : Dhcpv4SrvTest(),
          iface_mgr_test_config_(true) {
        IfaceMgr::instance().openSockets4();
    }

    /// @brief Interface Manager's fake configuration control.
    IfaceMgrTestConfig iface_mgr_test_config_;

};

/// This test verifies that the client in the SELECTING state can get
/// an address when it doesn't request any specific address in the
/// DHCPDISCOVER message.
TEST_F(DORATest, selectingDoNotRequestAddress) {
    Dhcp4Client client(Dhcp4Client::SELECTING);
    // Configure DHCP server.
    configure(DORA_CONFIGS[0], *client.getServer());

    // Perform 4-way exchange with the server but to not request any
    // specific address in the DHCPDISCOVER message.
    ASSERT_NO_THROW(client.doDORA());

    // Make sure that the server responded.
    ASSERT_TRUE(client.getContext().response_);
    Pkt4Ptr resp = client.getContext().response_;
    // Make sure that the server has responded with DHCPACK.
    ASSERT_EQ(DHCPACK, static_cast<int>(resp->getType()));
    // Response must not be relayed.
    EXPECT_FALSE(resp->isRelayed());
    // Make sure that the server id is present.
    EXPECT_EQ("10.0.0.1", client.config_.serverid_.toText());
    // Make sure that the client has got the lease with the requested address.
    ASSERT_NE(client.config_.lease_.addr_.toText(), "0.0.0.0");
}

/// This test verifies that multiple clients may use the DHCPv4 server
/// and obtain unique leases.
TEST_F(DORATest, selectingMultipleClients) {
    Dhcp4Client client(Dhcp4Client::SELECTING);
    // Configure DHCP server.
    configure(DORA_CONFIGS[0], *client.getServer());

    // Get the first lease.
    ASSERT_NO_THROW(client.doDORA());

    // Make sure that the server responded.
    Pkt4Ptr resp = client.getContext().response_;
    ASSERT_TRUE(resp);
    ASSERT_EQ(DHCPACK, static_cast<int>(resp->getType()));
    // Store the lease.
    Lease4 lease1 = client.config_.lease_;

    // Get the lease for a different client.
    client.modifyHWAddr();
    ASSERT_NO_THROW(client.doDORA());
    // Make sure that the server responded.
    resp = client.getContext().response_;
    ASSERT_TRUE(resp);
    ASSERT_EQ(DHCPACK, static_cast<int>(resp->getType()));
    // Store the lease.
    Lease4 lease2 = client.config_.lease_;

    // Get the lease for a different client.
    client.modifyHWAddr();
    ASSERT_NO_THROW(client.doDORA());
    // Make sure that the server responded.
    resp = client.getContext().response_;
    ASSERT_TRUE(resp);
    ASSERT_EQ(DHCPACK, static_cast<int>(resp->getType()));
    // Store the lease.
    Lease4 lease3 = client.config_.lease_;

    // Make sure that unique addresses have been assigned.
    EXPECT_NE(lease1.addr_, lease2.addr_);
    EXPECT_NE(lease2.addr_, lease3.addr_);
    EXPECT_NE(lease1.addr_, lease3.addr_);
}

// This test verifies that the client in a SELECTING state can request
// a specific address and that this address will be assigned when
// available. It also tests that if the client requests an address which
// is in use the client will get a different address.
TEST_F(DORATest, selectingRequestAddress) {
    Dhcp4Client client(Dhcp4Client::SELECTING);
    // Configure DHCP server.
    configure(DORA_CONFIGS[0], *client.getServer());

    // Perform 4-way exchange with the server.
    ASSERT_NO_THROW(client.doDORA(boost::shared_ptr<
                                  IOAddress>(new IOAddress("10.0.0.50"))));

    // Make sure that the server responded.
    ASSERT_TRUE(client.getContext().response_);
    Pkt4Ptr resp = client.getContext().response_;
    // Make sure that the server has responded with DHCPACK.
    ASSERT_EQ(DHCPACK, static_cast<int>(resp->getType()));
    // Response must not be relayed.
    EXPECT_FALSE(resp->isRelayed());
    // Make sure that the server id is present.
    EXPECT_EQ("10.0.0.1", client.config_.serverid_.toText());
    // Make sure that the client has got the lease with the requested address.
    ASSERT_EQ("10.0.0.50", client.config_.lease_.addr_.toText());

    // Simulate different client requesting the same address.
    client.modifyHWAddr();
    ASSERT_NO_THROW(client.doDORA(boost::shared_ptr<
                                  IOAddress>(new IOAddress("10.0.0.50"))));
    resp = client.getContext().response_;
    // Make sure that the server responded.
    ASSERT_TRUE(resp);
    // Make sure that the server has responded with DHCPACK.
    ASSERT_EQ(DHCPACK, static_cast<int>(resp->getType()));
    // Response must not be relayed.
    EXPECT_FALSE(resp->isRelayed());
    // Make sure that the server id is present.
    EXPECT_EQ("10.0.0.1", client.config_.serverid_.toText());
    // Make sure that the client has got some address.
    EXPECT_NE(client.config_.lease_.addr_.toText(), "0.0.0.0");
    // Make sure that the client has got a different address than requested
    // as the requested one is already in use.
    EXPECT_NE(client.config_.lease_.addr_.toText(), "10.0.0.50");
}

// This test verifies that the client will get the address that it has
// been allocated when the client requests a different address.
TEST_F(DORATest, selectingRequestNonMatchingAddress) {
    Dhcp4Client client(Dhcp4Client::SELECTING);
    // Configure DHCP server.
    configure(DORA_CONFIGS[0], *client.getServer());

    // Perform 4-way exchange with the server.
    ASSERT_NO_THROW(client.doDORA(boost::shared_ptr<
                                  IOAddress>(new IOAddress("10.0.0.50"))));

    // Make sure that the server responded.
    ASSERT_TRUE(client.getContext().response_);
    Pkt4Ptr resp = client.getContext().response_;
    // Make sure that the server has responded with DHCPACK.
    ASSERT_EQ(DHCPACK, static_cast<int>(resp->getType()));
    // Response must not be relayed.
    EXPECT_FALSE(resp->isRelayed());
    // Make sure that the server id is present.
    EXPECT_EQ("10.0.0.1", client.config_.serverid_.toText());
    // Make sure that the client has got the lease with the requested address.
    ASSERT_EQ("10.0.0.50", client.config_.lease_.addr_.toText());

    // Let's request a different address. The server should respond with
    // the one that the client already has allocated.
    ASSERT_NO_THROW(client.doDORA(boost::shared_ptr<
                                  IOAddress>(new IOAddress("10.0.0.80"))));
    // Make sure that the server responded.
    ASSERT_TRUE(client.getContext().response_);
    resp = client.getContext().response_;
    // Make sure that the server has responded with DHCPACK.
    ASSERT_EQ(DHCPACK, static_cast<int>(resp->getType()));
    // Response must not be relayed.
    EXPECT_FALSE(resp->isRelayed());
    // Make sure that the server id is present.
    EXPECT_EQ("10.0.0.1", client.config_.serverid_.toText());
    // Make sure that the client has got the lease with the address that
    // the client has recorded in the lease database.
    EXPECT_EQ("10.0.0.50", client.config_.lease_.addr_.toText());
}

// Test that the client in the INIT-REBOOT state can request the IP
// address it has and the address is returned. Also, check that if
// if the client requests invalid address the server sends a DHCPNAK.
TEST_F(DORATest, initRebootRequest) {
    Dhcp4Client client(Dhcp4Client::SELECTING);
    // Configure DHCP server.
    configure(DORA_CONFIGS[0], *client.getServer());
    // Obtain a lease from the server using the 4-way exchange.
    ASSERT_NO_THROW(client.doDORA(boost::shared_ptr<
                                  IOAddress>(new IOAddress("10.0.0.50"))));
    // Make sure that the server responded.
    ASSERT_TRUE(client.getContext().response_);
    Pkt4Ptr resp = client.getContext().response_;
    // Make sure that the server has responded with DHCPACK.
    ASSERT_EQ(DHCPACK, static_cast<int>(resp->getType()));
    // Response must not be relayed.
    EXPECT_FALSE(resp->isRelayed());
    // Make sure that the server id is present.
    EXPECT_EQ("10.0.0.1", client.config_.serverid_.toText());
    // Make sure that the client has got the lease with the requested address.
    ASSERT_EQ("10.0.0.50", client.config_.lease_.addr_.toText());

    // Client has a lease in the database. Let's transition the client
    // to the INIT_REBOOT state so as the client can request the cached
    // lease using the DHCPREQUEST message.
    client.setState(Dhcp4Client::INIT_REBOOT);
    ASSERT_NO_THROW(client.doRequest());

    // Make sure that the server responded.
    ASSERT_TRUE(client.getContext().response_);
    resp = client.getContext().response_;
    // Make sure that the server has responded with DHCPACK.
    ASSERT_EQ(DHCPACK, static_cast<int>(resp->getType()));
    // Response must not be relayed.
    EXPECT_FALSE(resp->isRelayed());
    // Make sure that the server id is present.
    EXPECT_EQ("10.0.0.1", client.config_.serverid_.toText());
    // Make sure that the client has got the lease with the requested address.
    ASSERT_EQ("10.0.0.50", client.config_.lease_.addr_.toText());

    // Try to request a different address than the client has. The server
    // should respond with DHCPNAK.
    client.config_.lease_.addr_ = IOAddress("10.0.0.30");
    ASSERT_NO_THROW(client.doRequest());
    // Make sure that the server responded.
    ASSERT_TRUE(client.getContext().response_);
    resp = client.getContext().response_;
    EXPECT_EQ(DHCPNAK, static_cast<int>(resp->getType()));

    // Try to request from a different client.
    client.modifyHWAddr();
    ASSERT_NO_THROW(client.doRequest());
    // The server should not respond.
    EXPECT_FALSE(client.getContext().response_);
}

// Check that the ciaddr returned by the server is correct for DHCPOFFER and
// DHCPNAK according to RFC2131, section 4.3.1.
TEST_F(DORATest, ciaddr) {
    Dhcp4Client client(Dhcp4Client::SELECTING);
    // Configure DHCP server.
    configure(DORA_CONFIGS[0], *client.getServer());
    // Force ciaddr of Discover message to be non-zero.
    client.ciaddr_.specify(IOAddress("10.0.0.50"));
    // Obtain a lease from the server using the 4-way exchange.
    ASSERT_NO_THROW(client.doDiscover(boost::shared_ptr<
                                      IOAddress>(new IOAddress("10.0.0.50"))));
    // Make sure that the server responded.
    ASSERT_TRUE(client.getContext().response_);
    Pkt4Ptr resp = client.getContext().response_;
    // Make sure that the server has responded with DHCPOFFER.
    ASSERT_EQ(DHCPOFFER, static_cast<int>(resp->getType()));
    // Make sure ciaddr is not set for DHCPOFFER.
    EXPECT_EQ("0.0.0.0", resp->getCiaddr().toText());

    // Obtain a lease from the server using the 4-way exchange.
    ASSERT_NO_THROW(client.doRequest());
    // Make sure that the server responded.
    ASSERT_TRUE(client.getContext().response_);
    resp = client.getContext().response_;
    // Make sure that the server has responded with DHCPACK.
    ASSERT_EQ(DHCPACK, static_cast<int>(resp->getType()));

    // Let's transition the client to Renewing state.
    client.setState(Dhcp4Client::RENEWING);

    // Set the unicast destination address to indicate that it is a renewal.
    client.setDestAddress(IOAddress("10.0.0.1"));
    ASSERT_NO_THROW(client.doRequest());
    // The client is sending invalid ciaddr so the server should send a NAK.
    resp = client.getContext().response_;
    ASSERT_EQ(DHCPACK, static_cast<int>(resp->getType()));
    // For DHCPACK the ciaddr may be 0 or may be set to the ciaddr value
    // from the client's message. Kea sets it to the latter.
    EXPECT_EQ("10.0.0.50", resp->getCiaddr().toText());

    // Replace the address held by the client. The client will request
    // the assignment of this address but the server has a different
    // address for this client.
    client.ciaddr_.specify(IOAddress("192.168.0.30"));
    ASSERT_NO_THROW(client.doRequest());
    // The client is sending invalid ciaddr so the server should send a NAK.
    resp = client.getContext().response_;
    ASSERT_EQ(DHCPNAK, static_cast<int>(resp->getType()));
    // For DHCPNAK the ciaddr is always 0 (should not be copied) from the
    // client's message.
    EXPECT_EQ("0.0.0.0", resp->getCiaddr().toText());
}

// This is a simple test for the host reservation. It creates a reservation
// for an address for a single client, identified by the HW address. The
// test verifies that the client using this HW address will obtain a
// lease for the reserved address. It also checks that the client using
// a different HW address will obtain an address from the dynamic pool.
TEST_F(DORATest, reservation) {
    // Client A is a one which will have a reservation.
    Dhcp4Client clientA(Dhcp4Client::SELECTING);
    // Set explicit HW address so as it matches the reservation in the
    // configuration used below.
    clientA.setHWAddress("aa:bb:cc:dd:ee:ff");
    // Configure DHCP server.
    configure(DORA_CONFIGS[2], *clientA.getServer());
    // Client A performs 4-way exchange and should obtain a reserved
    // address.
    ASSERT_NO_THROW(clientA.doDORA(boost::shared_ptr<
                                  IOAddress>(new IOAddress("0.0.0.0"))));
    // Make sure that the server responded.
    ASSERT_TRUE(clientA.getContext().response_);
    Pkt4Ptr resp = clientA.getContext().response_;
    // Make sure that the server has responded with DHCPACK.
    ASSERT_EQ(DHCPACK, static_cast<int>(resp->getType()));
    // Make sure that the client has got the lease for the reserved address.
    ASSERT_EQ("10.0.0.7", clientA.config_.lease_.addr_.toText());

    // Client B uses the same server as Client A.
    Dhcp4Client clientB(clientA.getServer(), Dhcp4Client::SELECTING);
    // Client B has no reservation so it should get the lease from
    // the dynamic pool.
    ASSERT_NO_THROW(clientB.doDORA(boost::shared_ptr<
                                  IOAddress>(new IOAddress("0.0.0.0"))));
    // Make sure that the server responded.
    ASSERT_TRUE(clientB.getContext().response_);
    resp = clientB.getContext().response_;
    // Make sure that the server has responded with DHCPACK.
    ASSERT_EQ(DHCPACK, static_cast<int>(resp->getType()));
    // Obtain the subnet to which the returned address belongs.
    Subnet4Ptr subnet = CfgMgr::instance().getCurrentCfg()->getCfgSubnets4()->
        selectSubnet(clientB.config_.lease_.addr_);
    ASSERT_TRUE(subnet);
    // Make sure that the address has been allocated from the dynamic pool.
    ASSERT_TRUE(subnet->inPool(Lease::TYPE_V4, clientB.config_.lease_.addr_));
}

// This test checks the following scenario:
// 1. Client A performs 4-way exchange and obtains a lease from the dynamic pool.
// 2. Reservation is created for the client A using an address out of the dynamic
//    pool.
// 3. Client A renews the lease.
// 4. Server responds with DHCPNAK to indicate that the client should stop using
//    an address for which it has a lease. Server doesn't want to renew an
//    address for which the client doesn't have a reservation, while it has
//    a reservation for a different address.
// 5. Client A receives a DHCPNAK and returns to the DHCP server discovery.
// 6. Client A performs a 4-way exchange with a server and the server allocates
//   a reserved address to the Client A.
// 7. Client A renews the allocated address and the server returns a DHCPACK.
// 8. Reservation for the Client A is removed.
// 9. Client A renews the (previously reserved) lease and the server returns
//    DHCPNAK because the address in use is neither reserved nor belongs to
//    the dynamic pool.
// 10. Client A returns to the DHCP server discovery.
// 11. Client A uses 4-way exchange to obtain a lease from the dynamic pool.
// 12. The new address that the Client A is using is reserved for Client B.
//     Client A still holds this address.
// 13. Client B uses 4-way exchange to obtain a new lease.
// 14. The server determines that the Client B has a reservation for the
//     address which is in use by Client A. The server drops the client's
//     DHCPDISCOVER message.
// 15. Client A renews the lease.
// 16. The server determines that the address that Client A is using is reserved
//     for Client B. The server returns DHCPNAK to the Client A.
// 17. Client B uses 4-way exchange to obtain the reserved lease but the lease
//     for the Client A hasn't been removed yet. Client B's DHCPDISCOVER
//     message is dropped again.
// 18. Client A uses 4-way exchange to allocate a new lease.
// 19. The server allocates a new lease from the dynamic pool but it avoids
//     allocating the address reserved for the Client B.
// 20. Client B uses 4-way exchange to obtain a new lease.
// 21. The server finally allocates a reserved address to the Client B.
TEST_F(DORATest, reservationsWithConflicts) {
    Dhcp4Client client(Dhcp4Client::SELECTING);
    // Configure DHCP server.
    configure(DORA_CONFIGS[0], *client.getServer());
    // Client A performs 4-way exchange and obtains a lease from the
    // dynamic pool.
    ASSERT_NO_THROW(client.doDORA(boost::shared_ptr<
                                  IOAddress>(new IOAddress("10.0.0.50"))));
    // Make sure that the server responded.
    ASSERT_TRUE(client.getContext().response_);
    Pkt4Ptr resp = client.getContext().response_;
    // Make sure that the server has responded with DHCPACK.
    ASSERT_EQ(DHCPACK, static_cast<int>(resp->getType()));
    // Make sure that the client has got the lease with the requested address.
    ASSERT_EQ("10.0.0.50", client.config_.lease_.addr_.toText());

    configure(DORA_CONFIGS[0], false);
    // Reservation is created for the client A using an address out of the
    // dynamic pool.
    HostPtr host(new Host(&client.getHWAddress()->hwaddr_[0],
                          client.getHWAddress()->hwaddr_.size(),
                          Host::IDENT_HWADDR, SubnetID(1),
                          SubnetID(0), IOAddress("10.0.0.9")));
    CfgMgr::instance().getStagingCfg()->getCfgHosts()->add(host);
    CfgMgr::instance().commit();

    // Let's transition the client to Renewing state.
    client.setState(Dhcp4Client::RENEWING);

    // Set the unicast destination address to indicate that it is a renewal.
    client.setDestAddress(IOAddress("10.0.0.1"));
    ASSERT_NO_THROW(client.doRequest());

    // Client should get the DHCPNAK from the server because the client has
    // a reservation for a different address that it is trying to renew.
    resp = client.getContext().response_;
    ASSERT_EQ(DHCPNAK, static_cast<int>(resp->getType()));

    // A conforming client would go back to the server discovery.
    client.setState(Dhcp4Client::SELECTING);
    // Obtain a lease from the server using the 4-way exchange.
    ASSERT_NO_THROW(client.doDORA(boost::shared_ptr<
                                  IOAddress>(new IOAddress("0.0.0.0"))));
    // Make sure that the server responded.
    ASSERT_TRUE(client.getContext().response_);
    resp = client.getContext().response_;
    // Make sure that the server has responded with DHCPACK with a reserved
    // address
    ASSERT_EQ(DHCPACK, static_cast<int>(resp->getType()));
    ASSERT_EQ("10.0.0.9", client.config_.lease_.addr_.toText());

    // Client A renews the allocated address.
    client.setState(Dhcp4Client::RENEWING);
    // Set the unicast destination address to indicate that it is a renewal.
    client.setDestAddress(IOAddress("10.0.0.1"));
    ASSERT_NO_THROW(client.doRequest());
    // Make sure the server responded and renewed the client's address.
    resp = client.getContext().response_;
    ASSERT_EQ("10.0.0.9", client.config_.lease_.addr_.toText());

    // By reconfiguring the server, we remove the existing reservations.
    configure(DORA_CONFIGS[0]);

    // Try to renew the existing lease again.
    ASSERT_NO_THROW(client.doRequest());
    // The reservation has been removed, so the server should respond with
    // a DHCPNAK because the address that the client is using doesn't belong
    // to a dynamic pool.
    resp = client.getContext().response_;
    ASSERT_EQ(DHCPNAK, static_cast<int>(resp->getType()));

    // A conforming client would go back to the server discovery.
    client.setState(Dhcp4Client::SELECTING);
    // Obtain a lease from the server using the 4-way exchange.
    ASSERT_NO_THROW(client.doDORA(boost::shared_ptr<
                                  IOAddress>(new IOAddress("0.0.0.0"))));
    // Make sure that the server responded.
    ASSERT_TRUE(client.getContext().response_);
    resp = client.getContext().response_;
    // Make sure that the server has responded with DHCPACK.
    ASSERT_EQ(DHCPACK, static_cast<int>(resp->getType()));
    // Obtain the subnet to which the returned address belongs.
    Subnet4Ptr subnet = CfgMgr::instance().getCurrentCfg()->getCfgSubnets4()->
        selectSubnet(client.config_.lease_.addr_);
    ASSERT_TRUE(subnet);
    // Make sure that the address has been allocated from the dynamic pool.
    ASSERT_TRUE(subnet->inPool(Lease::TYPE_V4, client.config_.lease_.addr_));

    // Remember the address allocated in the dynamic pool.
    IOAddress in_pool_addr = client.config_.lease_.addr_;

    // Create Client B.
    Dhcp4Client clientB(client.getServer());
    clientB.modifyHWAddr();

    // Create reservation for the Client B, for the address that the
    // Client A is using.
    configure(DORA_CONFIGS[0], false);
    host.reset(new Host(&clientB.getHWAddress()->hwaddr_[0],
                        clientB.getHWAddress()->hwaddr_.size(),
                        Host::IDENT_HWADDR, SubnetID(1),
                        SubnetID(0), in_pool_addr));
    CfgMgr::instance().getStagingCfg()->getCfgHosts()->add(host);
    CfgMgr::instance().commit();

    // Client B performs a DHCPDISCOVER.
    clientB.setState(Dhcp4Client::SELECTING);
    // The server determines that the address reserved for Client B is
    // in use by Client A so it drops the message from the Client B.
    ASSERT_NO_THROW(clientB.doDiscover(boost::shared_ptr<
                                       IOAddress>(new IOAddress("0.0.0.0"))));
    ASSERT_FALSE(clientB.getContext().response_);

    // Client A renews the lease.
    client.setState(Dhcp4Client::RENEWING);
    // Set the unicast destination address to indicate that it is a renewal.
    client.setDestAddress(IOAddress(in_pool_addr));
    ASSERT_NO_THROW(client.doRequest());
    // Client A should get a DHCPNAK because it is using an address reserved
    // for Client B.
    resp = client.getContext().response_;
    ASSERT_EQ(DHCPNAK, static_cast<int>(resp->getType()));

    // Client B performs 4-way exchange but still doesn't get an address
    // because Client A hasn't obtained a new lease, so it is still using
    // an address reserved for Client B.
    clientB.setState(Dhcp4Client::SELECTING);
    // Obtain a lease from the server using the 4-way exchange.
    ASSERT_NO_THROW(clientB.doDiscover(boost::shared_ptr<
                                       IOAddress>(new IOAddress("0.0.0.0"))));
    // Make sure that the server responded.
    ASSERT_FALSE(clientB.getContext().response_);

    // Client A performs 4-way exchange.
    client.setState(Dhcp4Client::SELECTING);
    // Obtain a lease from the server using the 4-way exchange.
    ASSERT_NO_THROW(client.doDORA(boost::shared_ptr<
                                  IOAddress>(new IOAddress("0.0.0.0"))));
    // Make sure that the server responded.
    ASSERT_TRUE(client.getContext().response_);
    resp = client.getContext().response_;
    ASSERT_EQ(DHCPACK, static_cast<int>(resp->getType()));
    // The server should have assigned a different address than the one
    // reserved for the Client B.
    ASSERT_NE(client.config_.lease_.addr_.toText(), in_pool_addr.toText());
    subnet = CfgMgr::instance().getCurrentCfg()->getCfgSubnets4()->
        selectSubnet(client.config_.lease_.addr_);
    ASSERT_TRUE(subnet);
    ASSERT_TRUE(subnet->inPool(Lease::TYPE_V4, client.config_.lease_.addr_));

    // Client B performs 4-way exchange and obtains a lease for the
    // reserved address.
    clientB.setState(Dhcp4Client::SELECTING);
    ASSERT_NO_THROW(clientB.doDORA(boost::shared_ptr<
                                  IOAddress>(new IOAddress("0.0.0.0"))));
    // Make sure that the server responded.
    ASSERT_TRUE(clientB.getContext().response_);
    resp = clientB.getContext().response_;
    ASSERT_EQ(DHCPACK, static_cast<int>(resp->getType()));
    ASSERT_EQ(in_pool_addr, clientB.config_.lease_.addr_);
}

} // end of anonymous namespace
