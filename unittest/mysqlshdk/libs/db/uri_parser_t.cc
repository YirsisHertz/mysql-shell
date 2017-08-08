/* Copyright (c) 2014, 2016, Oracle and/or its affiliates. All rights reserved.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include "unittest/gtest_clean.h"
#include "mysqlshdk/libs/db/uri_parser.h"
#include "mysqlshdk/libs/utils/nullable.h"
#include "mysqlshdk/libs/utils/utils_string.h"

using mysqlshdk::db::uri::Uri_parser;
using mysqlshdk::db::Transport_type;
namespace testing {
mysqlshdk::utils::nullable<const char *> no_string;
mysqlshdk::utils::nullable<int> no_int;

#define NO_SCHEMA no_string
#define NO_USER no_string
#define NO_PASSWORD no_string
#define NO_HOST no_string
#define NO_PORT no_int
#define NO_SOCK no_string
#define NO_DB no_string
#define HAS_PASSWORD true
#define HAS_NO_PASSWORD false
#define HAS_PORT true
#define HAS_NO_PORT false

namespace proj_parser_tests {
void validate_bad_uri(const std::string &connstring, const std::string &error) {
  SCOPED_TRACE(connstring);

  Uri_parser parser;

  try {
    parser.parse(connstring);

    if (!error.empty()) {
      SCOPED_TRACE("MISSING ERROR: " + error);
      FAIL();
    }
  } catch (std::invalid_argument &err) {
    std::string found_error(err.what());

    if (error.empty()) {
      SCOPED_TRACE("UNEXPECTED ERROR: " + found_error);
      FAIL();
    } else if (found_error.find(error) == std::string::npos) {
      SCOPED_TRACE("ACTUAL: " + found_error);
      SCOPED_TRACE("EXPECTED: " + error);
      FAIL();
    }
  }
}

void validate_uri(
    const std::string &connstring,
    const mysqlshdk::utils::nullable<const char *> &scheme,
    const mysqlshdk::utils::nullable<const char *> &user,
    const mysqlshdk::utils::nullable<const char *> &password,
    const mysqlshdk::utils::nullable<const char *> &host,
    const mysqlshdk::utils::nullable<int> &port,
    const mysqlshdk::utils::nullable<const char *> &sock,
    const mysqlshdk::utils::nullable<const char *> &db, bool has_password,
    bool has_port, Transport_type target_type,
    const std::map<std::string, std::vector<std::string> > *attributes = 0) {
  SCOPED_TRACE(connstring);

  Uri_parser parser;

  try {
    mysqlshdk::db::Connection_options data = parser.parse(connstring);

    if (scheme.is_null())
      ASSERT_FALSE(data.has_scheme());
    else
      ASSERT_STREQ(*scheme, data.get_scheme().c_str());

    if (user.is_null())
      ASSERT_FALSE(data.has_user());
    else
      ASSERT_STREQ(*user, data.get_user().c_str());

    if (password.is_null())
      ASSERT_FALSE(data.has_password());
    else
      ASSERT_STREQ(*password, data.get_password().c_str());

    // TODO(rennox): The callers should be updated to indicate IF
    // transport type should actually be defined for each case
    if (data.has_transport_type()) {
      ASSERT_EQ(target_type, data.get_transport_type());

      switch (data.get_transport_type()) {
        case Transport_type::Tcp:
          if (host.is_null())
            ASSERT_FALSE(data.has_password());
          else
            ASSERT_STREQ(*host, data.get_host().c_str());

          if (port.is_null())
            ASSERT_FALSE(data.has_port());
          else
            ASSERT_EQ(*port, data.get_port());
          break;
        case Transport_type::Socket:
          if (sock.is_null())
            ASSERT_FALSE(data.has_socket());
          else
            ASSERT_STREQ(*sock, data.get_socket().c_str());
          break;
        case Transport_type::Pipe:
          if (sock.is_null())
            ASSERT_FALSE(data.has_socket());
          else
            ASSERT_STREQ(*sock, data.get_pipe().c_str());
          break;
      }
    }

    if (db.is_null())
      ASSERT_FALSE(data.has_schema());
    else
      ASSERT_STREQ(*db, data.get_schema().c_str());

    if (attributes) {
      for (auto att : *attributes) {
        SCOPED_TRACE(att.first);

        std::string atts;
        if (att.second.size() == 1)
          atts = att.second[0];
        else
          atts = "[" + shcore::str_join(att.second, ",") + "]";
        ASSERT_STREQ(atts.c_str(), data.get(att.first).c_str());
      }
    }
  } catch (std::invalid_argument &err) {
    std::string found_error(err.what());

    SCOPED_TRACE("UNEXPECTED ERROR: " + found_error);
    FAIL();
  }
}

void validate_ipv6(const std::string &address) {
  std::string result = address.substr(1, address.length() - 2);
  validate_uri("mysqlx://user:sample@" + address + ":2540/testing", "mysqlx",
               "user", "sample", result.c_str(), 2540, NO_SOCK, "testing",
               HAS_PASSWORD, HAS_PORT, Transport_type::Tcp);
}

TEST(Uri_parser, parse_scheme) {
  validate_uri("mysqlx://mysql.com", "mysqlx", NO_USER, NO_PASSWORD,
               "mysql.com", NO_PORT, NO_SOCK, NO_DB, HAS_NO_PASSWORD,
               HAS_NO_PORT, Transport_type::Tcp);
  validate_uri("mysql://mysql.com", "mysql", NO_USER, NO_PASSWORD, "mysql.com",
               NO_PORT, NO_SOCK, NO_DB, HAS_NO_PASSWORD, HAS_NO_PORT,
               Transport_type::Tcp);
  validate_uri("mysql.com", NO_SCHEMA, NO_USER, NO_PASSWORD, "mysql.com",
               NO_PORT, NO_SOCK, NO_DB, HAS_NO_PASSWORD, HAS_NO_PORT,
               Transport_type::Tcp);

  //                0    0    1    1    2    2    3    3    4    4    5    5
  //                0    5    0    5    0    5    0    5    0    5    0    5
  validate_bad_uri("://mysql.com", "Scheme is missing");
  validate_bad_uri(
      "other://mysql.com",
      "Invalid scheme [other], supported schemes include: mysql, mysqlx");
  validate_bad_uri("mysqlx ://mysql.com", "Illegal space found at position 6");
  validate_bad_uri("mysq=lx://mysql.com",
                   "Illegal character [=] found at position 4");
  validate_bad_uri("mysqlx+ssh+other://mysql.com",
                   "Invalid scheme format [mysqlx+ssh+other], only one "
                   "extension is supported");
  validate_bad_uri("mysqlx+ssh://mysql.com",
                   "Scheme extension [ssh] is not supported");
}

TEST(Uri_parser, parse_user_info) {
  validate_uri("mysqlx://user@mysql.com", "mysqlx", "user", NO_PASSWORD,
               "mysql.com", NO_PORT, NO_SOCK, NO_DB, HAS_NO_PASSWORD,
               HAS_NO_PORT, Transport_type::Tcp);
  validate_uri("mysqlx://user.name@mysql.com", "mysqlx", "user.name",
               NO_PASSWORD, "mysql.com", NO_PORT, NO_SOCK, NO_DB,
               HAS_NO_PASSWORD, HAS_NO_PORT, Transport_type::Tcp);
  validate_uri("mysqlx://user-name@mysql.com", "mysqlx", "user-name",
               NO_PASSWORD, "mysql.com", NO_PORT, NO_SOCK, NO_DB,
               HAS_NO_PASSWORD, HAS_NO_PORT, Transport_type::Tcp);
  validate_uri("mysqlx://user~name@mysql.com", "mysqlx", "user~name",
               NO_PASSWORD, "mysql.com", NO_PORT, NO_SOCK, NO_DB,
               HAS_NO_PASSWORD, HAS_NO_PORT, Transport_type::Tcp);
  validate_uri("mysqlx://user!name@mysql.com", "mysqlx", "user!name",
               NO_PASSWORD, "mysql.com", NO_PORT, NO_SOCK, NO_DB,
               HAS_NO_PASSWORD, HAS_NO_PORT, Transport_type::Tcp);
  validate_uri("mysqlx://user$name@mysql.com", "mysqlx", "user$name",
               NO_PASSWORD, "mysql.com", NO_PORT, NO_SOCK, NO_DB,
               HAS_NO_PASSWORD, HAS_NO_PORT, Transport_type::Tcp);
  validate_uri("mysqlx://user&name@mysql.com", "mysqlx", "user&name",
               NO_PASSWORD, "mysql.com", NO_PORT, NO_SOCK, NO_DB,
               HAS_NO_PASSWORD, HAS_NO_PORT, Transport_type::Tcp);
  validate_uri("mysqlx://user'name@mysql.com", "mysqlx", "user'name",
               NO_PASSWORD, "mysql.com", NO_PORT, NO_SOCK, NO_DB,
               HAS_NO_PASSWORD, HAS_NO_PORT, Transport_type::Tcp);
  validate_uri("mysqlx://user(name)@mysql.com", "mysqlx", "user(name)",
               NO_PASSWORD, "mysql.com", NO_PORT, NO_SOCK, NO_DB,
               HAS_NO_PASSWORD, HAS_NO_PORT, Transport_type::Tcp);
  validate_uri("mysqlx://user*name@mysql.com", "mysqlx", "user*name",
               NO_PASSWORD, "mysql.com", NO_PORT, NO_SOCK, NO_DB,
               HAS_NO_PASSWORD, HAS_NO_PORT, Transport_type::Tcp);
  validate_uri("mysqlx://user+name@mysql.com", "mysqlx", "user+name",
               NO_PASSWORD, "mysql.com", NO_PORT, NO_SOCK, NO_DB,
               HAS_NO_PASSWORD, HAS_NO_PORT, Transport_type::Tcp);
  validate_uri("mysqlx://user,name@mysql.com", "mysqlx", "user,name",
               NO_PASSWORD, "mysql.com", NO_PORT, NO_SOCK, NO_DB,
               HAS_NO_PASSWORD, HAS_NO_PORT, Transport_type::Tcp);
  validate_uri("mysqlx://user;name@mysql.com", "mysqlx", "user;name",
               NO_PASSWORD, "mysql.com", NO_PORT, NO_SOCK, NO_DB,
               HAS_NO_PASSWORD, HAS_NO_PORT, Transport_type::Tcp);
  validate_uri("mysqlx://user=name@mysql.com", "mysqlx", "user=name",
               NO_PASSWORD, "mysql.com", NO_PORT, NO_SOCK, NO_DB,
               HAS_NO_PASSWORD, HAS_NO_PORT, Transport_type::Tcp);
  validate_uri("mysqlx://user%2Aname@mysql.com", "mysqlx", "user*name",
               NO_PASSWORD, "mysql.com", NO_PORT, NO_SOCK, NO_DB,
               HAS_NO_PASSWORD, HAS_NO_PORT, Transport_type::Tcp);
  validate_uri("mysqlx://user%2Aname:password@mysql.com", "mysqlx", "user*name",
               "password", "mysql.com", NO_PORT, NO_SOCK, NO_DB, HAS_PASSWORD,
               HAS_NO_PORT, Transport_type::Tcp);

  //                0    0    1    1    2    2    3    3    4    4    5    5
  //                0    5    0    5    0    5    0    5    0    5    0    5
  validate_bad_uri("mysqlx://@mysql.com", "Missing user information");
  validate_bad_uri("mysqlx ://user_name@mysql.com",
                   "Illegal space found at position 6");
  validate_bad_uri("mysqlx://user%name@mysql.com",
                   "Illegal character [%] found at position 13");
  validate_bad_uri("mysqlx://user%1name@mysql.com",
                   "Illegal character [%] found at position 13");
  validate_bad_uri("mysqlx://user%2Aname:p@ssword@mysql.com",
                   "Illegal character [@] found at position 29");
  validate_bad_uri("mysqlx://:password@mysql.com", "Missing user name");
}

TEST(Uri_parser, parse_host_name) {
#ifdef WIN32
  validate_uri("mysqlx://user@localhost", "mysqlx", "user", NO_PASSWORD,
               "localhost", NO_PORT, NO_SOCK, NO_DB, HAS_NO_PASSWORD,
               HAS_NO_PORT, Transport_type::Tcp);
#else
  validate_uri("mysqlx://user@localhost", "mysqlx", "user", NO_PASSWORD,
               "localhost", NO_PORT, NO_SOCK, NO_DB, HAS_NO_PASSWORD,
               HAS_NO_PORT, Transport_type::Pipe);
#endif
  validate_uri("mysqlx://user@localhost:3306", "mysqlx", "user", NO_PASSWORD,
               "localhost", 3306, NO_SOCK, NO_DB, HAS_NO_PASSWORD, HAS_PORT,
               Transport_type::Tcp);
  validate_uri("mysqlx://user@mysql.com", "mysqlx", "user", NO_PASSWORD,
               "mysql.com", NO_PORT, NO_SOCK, NO_DB, HAS_NO_PASSWORD,
               HAS_NO_PORT, Transport_type::Tcp);
  validate_uri("mysqlx://user@mysql1.com", "mysqlx", "user", NO_PASSWORD,
               "mysql1.com", NO_PORT, NO_SOCK, NO_DB, HAS_NO_PASSWORD,
               HAS_NO_PORT, Transport_type::Tcp);
  validate_uri("mysqlx://user@mysql1-com", "mysqlx", "user", NO_PASSWORD,
               "mysql1-com", NO_PORT, NO_SOCK, NO_DB, HAS_NO_PASSWORD,
               HAS_NO_PORT, Transport_type::Tcp);
  validate_uri("mysqlx://user@mysql1~com", "mysqlx", "user", NO_PASSWORD,
               "mysql1~com", NO_PORT, NO_SOCK, NO_DB, HAS_NO_PASSWORD,
               HAS_NO_PORT, Transport_type::Tcp);
  validate_uri("mysqlx://user@mysql1_com", "mysqlx", "user", NO_PASSWORD,
               "mysql1_com", NO_PORT, NO_SOCK, NO_DB, HAS_NO_PASSWORD,
               HAS_NO_PORT, Transport_type::Tcp);
  validate_uri("mysqlx://user@mysql1!com", "mysqlx", "user", NO_PASSWORD,
               "mysql1!com", NO_PORT, NO_SOCK, NO_DB, HAS_NO_PASSWORD,
               HAS_NO_PORT, Transport_type::Tcp);
  validate_uri("mysqlx://user@mysql1$com", "mysqlx", "user", NO_PASSWORD,
               "mysql1$com", NO_PORT, NO_SOCK, NO_DB, HAS_NO_PASSWORD,
               HAS_NO_PORT, Transport_type::Tcp);
  validate_uri("mysqlx://user@mysql1&com", "mysqlx", "user", NO_PASSWORD,
               "mysql1&com", NO_PORT, NO_SOCK, NO_DB, HAS_NO_PASSWORD,
               HAS_NO_PORT, Transport_type::Tcp);
  validate_uri("mysqlx://user@mysql1'com", "mysqlx", "user", NO_PASSWORD,
               "mysql1'com", NO_PORT, NO_SOCK, NO_DB, HAS_NO_PASSWORD,
               HAS_NO_PORT, Transport_type::Tcp);
  validate_uri("mysqlx://user@mysql1(com)", "mysqlx", "user", NO_PASSWORD,
               "mysql1(com)", NO_PORT, NO_SOCK, NO_DB, HAS_NO_PASSWORD,
               HAS_NO_PORT, Transport_type::Tcp);
  validate_uri("mysqlx://user@mysql1*com", "mysqlx", "user", NO_PASSWORD,
               "mysql1*com", NO_PORT, NO_SOCK, NO_DB, HAS_NO_PASSWORD,
               HAS_NO_PORT, Transport_type::Tcp);
  validate_uri("mysqlx://user@mysql1+com", "mysqlx", "user", NO_PASSWORD,
               "mysql1+com", NO_PORT, NO_SOCK, NO_DB, HAS_NO_PASSWORD,
               HAS_NO_PORT, Transport_type::Tcp);
  validate_uri("mysqlx://user@mysql1,com", "mysqlx", "user", NO_PASSWORD,
               "mysql1,com", NO_PORT, NO_SOCK, NO_DB, HAS_NO_PASSWORD,
               HAS_NO_PORT, Transport_type::Tcp);
  validate_uri("mysqlx://user@mysql1;com", "mysqlx", "user", NO_PASSWORD,
               "mysql1;com", NO_PORT, NO_SOCK, NO_DB, HAS_NO_PASSWORD,
               HAS_NO_PORT, Transport_type::Tcp);
  validate_uri("mysqlx://user@mysql1=com", "mysqlx", "user", NO_PASSWORD,
               "mysql1=com", NO_PORT, NO_SOCK, NO_DB, HAS_NO_PASSWORD,
               HAS_NO_PORT, Transport_type::Tcp);
  validate_uri("mysqlx://user@mysql1%2ecom", "mysqlx", "user", NO_PASSWORD,
               "mysql1.com", NO_PORT, NO_SOCK, NO_DB, HAS_NO_PASSWORD,
               HAS_NO_PORT, Transport_type::Tcp);
  validate_uri("mysqlx://user@mysql1%2ecom:2845", "mysqlx", "user", NO_PASSWORD,
               "mysql1.com", 2845, NO_SOCK, NO_DB, HAS_NO_PASSWORD, HAS_PORT,
               Transport_type::Tcp);

  //                0    0    1    1    2    2    3    3    4    4    5    5
  //                0    5    0    5    0    5    0    5    0    5    0    5
  validate_bad_uri("mysqlx://user@mysql1%com",
                   "Illegal character [%] found at position 20");
  validate_bad_uri("mysqlx://user@mysql1%cgcom",
                   "Illegal character [%] found at position 20");
  validate_bad_uri("mysqlx://user@mysql1%2ecom:65538",
                   "Port is out of the valid range: 0 - 65535");
  validate_bad_uri("mysqlx://user@mysql1%2ecom:", "Missing port number");
  validate_bad_uri("mysqlx://user@mysql1%2ecom:2845f",
                   "Illegal character [f] found at position 31");
  validate_bad_uri("mysqlx://user@mysql1%2ecom:invalid",
                   "Illegal character [i] found at position 27");
}

TEST(Uri_parser, parse_host_ipv4) {
  validate_uri("mysqlx://user@10.150.123.45", "mysqlx", "user", NO_PASSWORD,
               "10.150.123.45", NO_PORT, NO_SOCK, NO_DB, HAS_NO_PASSWORD,
               HAS_NO_PORT, Transport_type::Tcp);
  validate_uri("mysqlx://user@10.150.123", "mysqlx", "user", NO_PASSWORD,
               "10.150.123", NO_PORT, NO_SOCK, NO_DB, HAS_NO_PASSWORD,
               HAS_NO_PORT, Transport_type::Tcp);
  validate_uri("mysqlx://user@10.150.123.whatever", "mysqlx", "user",
               NO_PASSWORD, "10.150.123.whatever", NO_PORT, NO_SOCK, NO_DB,
               HAS_NO_PASSWORD, HAS_NO_PORT, Transport_type::Tcp);
  validate_uri("mysqlx://user@0.0.0.0", "mysqlx", "user", NO_PASSWORD,
               "0.0.0.0", NO_PORT, NO_SOCK, NO_DB, HAS_NO_PASSWORD, HAS_NO_PORT,
               Transport_type::Tcp);
  validate_uri("mysqlx://user@255.255.255.255", "mysqlx", "user", NO_PASSWORD,
               "255.255.255.255", NO_PORT, NO_SOCK, NO_DB, HAS_NO_PASSWORD,
               HAS_NO_PORT, Transport_type::Tcp);
  validate_uri("mysqlx://user@10.150.123.45:2845", "mysqlx", "user",
               NO_PASSWORD, "10.150.123.45", 2845, NO_SOCK, NO_DB,
               HAS_NO_PASSWORD, HAS_PORT, Transport_type::Tcp);

  //                0    0    1    1    2    2    3    3    4    4    5    5
  //                0    5    0    5    0    5    0    5    0    5    0    5
  validate_bad_uri("mysqlx://user@256.255.255.255",
                   "Octect value out of bounds [256], valid range for IPv4 is "
                   "0 to 255 at position 14");
  validate_bad_uri("mysqlx://user@255.256.255.255",
                   "Octect value out of bounds [256], valid range for IPv4 is "
                   "0 to 255 at position 18");
  validate_bad_uri("mysqlx://user@255.255.256.255",
                   "Octect value out of bounds [256], valid range for IPv4 is "
                   "0 to 255 at position 22");
  validate_bad_uri("mysqlx://user@255.255.255.256",
                   "Octect value out of bounds [256], valid range for IPv4 is "
                   "0 to 255 at position 26");
  validate_bad_uri("mysqlx://user@10.150.123.45:68000",
                   "Port is out of the valid range: 0 - 65535");
  validate_bad_uri("mysqlx://user@10.150.123.45:", "Missing port number");
  validate_bad_uri("mysqlx://user@10.150.123.45:2845f",
                   "Illegal character [f] found at position 32");
  validate_bad_uri("mysqlx://user@10.150.123.45:invalid",
                   "Illegal character [i] found at position 28");
}

TEST(Uri_parser, parse_host_ipv6) {
  // Complete
  validate_ipv6("[1:2:3:4:5:6:7:8]");

  // Hiding 1
  validate_ipv6("[::1:2:3:4:5:6:7]");
  validate_ipv6("[1::2:3:4:5:6:7]");
  validate_ipv6("[1:2::3:4:5:6:7]");
  validate_ipv6("[1:2:3::4:5:6:7]");
  validate_ipv6("[1:2:3:4::5:6:7]");
  validate_ipv6("[1:2:3:4:5::6:7]");
  validate_ipv6("[1:2:3:4:5:6::7]");
  validate_ipv6("[1:2:3:4:5:6:7::]");

  // Hiding 2
  validate_ipv6("[::1:2:3:4:5:6]");
  validate_ipv6("[1::2:3:4:5:6]");
  validate_ipv6("[1:2::3:4:5:6]");
  validate_ipv6("[1:2:3::4:5:6]");
  validate_ipv6("[1:2:3:4::5:6]");
  validate_ipv6("[1:2:3:4:5::6]");
  validate_ipv6("[1:2:3:4:5:6::]");

  // Hiding 3
  validate_ipv6("[::1:2:3:4:5]");
  validate_ipv6("[1::2:3:4:5]");
  validate_ipv6("[1:2::3:4:5]");
  validate_ipv6("[1:2:3::4:5]");
  validate_ipv6("[1:2:3:4::5]");
  validate_ipv6("[1:2:3:4:5::]");

  // Hiding 4
  validate_ipv6("[::1:2:3:4]");
  validate_ipv6("[1::2:3:4]");
  validate_ipv6("[1:2::3:4]");
  validate_ipv6("[1:2:3::4]");
  validate_ipv6("[1:2:3:4::]");

  // Hiding 5
  validate_ipv6("[::1:2:3]");
  validate_ipv6("[1::2:3]");
  validate_ipv6("[1:2::3]");
  validate_ipv6("[1:2:3::]");

  // Hiding 6
  validate_ipv6("[::1:2]");
  validate_ipv6("[1::2]");
  validate_ipv6("[1:2::]");

  // Hiding 7
  validate_ipv6("[::1]");
  validate_ipv6("[1::]");

  // Hiding 8
  validate_ipv6("[::]");

  // IPv4 Embedded
  // Complete
  validate_ipv6("[1:2:3:4:5:6:1.2.3.4]");

  // Hiding 1
  validate_ipv6("[::1:2:3:4:5:1.2.3.4]");
  validate_ipv6("[1::2:3:4:5:1.2.3.4]");
  validate_ipv6("[1:2::3:4:5:1.2.3.4]");
  validate_ipv6("[1:2:3::4:5:1.2.3.4]");
  validate_ipv6("[1:2:3:4::5:1.2.3.4]");
  validate_ipv6("[1:2:3:4:5::1.2.3.4]");

  // Hiding 2
  validate_ipv6("[::1:2:3:4:1.2.3.4]");
  validate_ipv6("[1::2:3:4:1.2.3.4]");
  validate_ipv6("[1:2::3:4:1.2.3.4]");
  validate_ipv6("[1:2:3::4:1.2.3.4]");
  validate_ipv6("[1:2:3:4::1.2.3.4]");

  // Hiding 3
  validate_ipv6("[::1:2:3:1.2.3.4]");
  validate_ipv6("[1::2:3:1.2.3.4]");
  validate_ipv6("[1:2::3:1.2.3.4]");
  validate_ipv6("[1:2:3::1.2.3.4]");

  // Hiding 4
  validate_ipv6("[::1:2:1.2.3.4]");
  validate_ipv6("[1::2:1.2.3.4]");
  validate_ipv6("[1:2::1.2.3.4]");

  // Hiding 5
  validate_ipv6("[::1:1.2.3.4]");
  validate_ipv6("[1::1.2.3.4]");

  // Hiding 6
  validate_ipv6("[::1.2.3.4]");

  // Artistic Cases
  // Hexadecimals are OK
  validate_ipv6("[A:B:C:D:E:F:7:8]");
  //                0    0    1    1    2    2    3    3    4    4    5    5
  //                0    5    0    5    0    5    0    5    0    5    0    5
  validate_bad_uri("mysqlx://user:sample@[A:B:C:D:E:G:7:8]:2540/testing",
                   "Illegal character [G] found at position 32");
  validate_bad_uri(
      "mysqlx://user:sample@[A:B:C:D:E:A1B23:7:8]:2540/testing",
      "Invalid IPv6 value [A1B23], maximum 4 hexadecimal digits accepted");
  validate_bad_uri(
      "mysqlx://user:sample@[A:B:C:D:E:F:7:8:9]:2540/testing",
      "Invalid IPv6: the number of segments does not match the specification");
  validate_bad_uri(
      "mysqlx://user:sample@[::1:2:3:4:5:6:7:8]:2540/testing",
      "Invalid IPv6: the number of segments does not match the specification");
  validate_bad_uri("mysqlx://user:sample@[1::3:4::6:7:8]:2540/testing",
                   "Unexpected data [:] found at position 29");
}

TEST(Uri_parser, parse_socket) {
  //            0    0    1    1    2    2    3    3    4    4    5    5
  //            0    5    0    5    0    5    0    5    0    5    0    5
  validate_uri("mysqlx://user:password@/path%2Fto%2Fsocket/schema", "mysqlx",
               "user", "password", NO_HOST, NO_PORT, "/path/to/socket",
               "schema", HAS_PASSWORD, HAS_NO_PORT, Transport_type::Socket);
  validate_uri("mysqlx://user:password@/path%2Fto%2Fsocket/", "mysqlx", "user",
               "password", NO_HOST, NO_PORT, "/path/to/socket", NO_SCHEMA,
               HAS_PASSWORD, HAS_NO_PORT, Transport_type::Socket);
  validate_uri("mysqlx://user:password@/path%2Fto%2Fsocket", "mysqlx", "user",
               "password", NO_HOST, NO_PORT, "/path/to/socket", NO_SCHEMA,
               HAS_PASSWORD, HAS_NO_PORT, Transport_type::Socket);
  validate_uri("mysqlx://user:password@/socket/schema", "mysqlx", "user",
               "password", NO_HOST, NO_PORT, "/socket", "schema", HAS_PASSWORD,
               HAS_NO_PORT, Transport_type::Socket);
  validate_uri("mysqlx://user:password@/socket/", "mysqlx", "user", "password",
               NO_HOST, NO_PORT, "/socket", NO_SCHEMA, HAS_PASSWORD,
               HAS_NO_PORT, Transport_type::Socket);
  validate_uri("mysqlx://user:password@/socket", "mysqlx", "user", "password",
               NO_HOST, NO_PORT, "/socket", NO_SCHEMA, HAS_PASSWORD,
               HAS_NO_PORT, Transport_type::Socket);
  validate_uri("mysqlx://user:password@a/schema", "mysqlx", "user", "password",
               "a", NO_PORT, NO_SOCK, "schema", HAS_PASSWORD, HAS_NO_PORT,
               Transport_type::Tcp);
  validate_uri("mysqlx://user:password@a/", "mysqlx", "user", "password", "a",
               NO_PORT, NO_SOCK, NO_SCHEMA, HAS_PASSWORD, HAS_NO_PORT,
               Transport_type::Tcp);
  validate_uri("mysqlx://user:password@(/path/to/mysql.sock)/schema", "mysqlx",
               "user", "password", NO_HOST, NO_PORT, "/path/to/mysql.sock",
               "schema", HAS_PASSWORD, HAS_NO_PORT, Transport_type::Socket);
  validate_uri("mysqlx://user:password@(/path/to/mysql.sock)/", "mysqlx",
               "user", "password", NO_HOST, NO_PORT, "/path/to/mysql.sock",
               NO_SCHEMA, HAS_PASSWORD, HAS_NO_PORT, Transport_type::Socket);
  validate_uri("mysqlx://user:password@(/path/to/mysql.sock)", "mysqlx", "user",
               "password", NO_HOST, NO_PORT, "/path/to/mysql.sock", NO_SCHEMA,
               HAS_PASSWORD, HAS_NO_PORT, Transport_type::Socket);

  validate_uri("mysqlx://user:password@.%2Fpath%2Fto%2Fsocket/schema", "mysqlx",
               "user", "password", NO_HOST, NO_PORT, "./path/to/socket",
               "schema", HAS_PASSWORD, HAS_NO_PORT, Transport_type::Socket);
  validate_uri("mysqlx://user:password@.%2Fpath%2Fto%2Fsocket/", "mysqlx",
               "user", "password", NO_HOST, NO_PORT, "./path/to/socket",
               NO_SCHEMA, HAS_PASSWORD, HAS_NO_PORT, Transport_type::Socket);
  validate_uri("mysqlx://user:password@.%2Fpath%2Fto%2Fsocket", "mysqlx",
               "user", "password", NO_HOST, NO_PORT, "./path/to/socket",
               NO_SCHEMA, HAS_PASSWORD, HAS_NO_PORT, Transport_type::Socket);

  validate_uri("mysqlx://user:password@..%2Fpath%2Fto%2Fsocket/schema",
               "mysqlx", "user", "password", NO_HOST, NO_PORT,
               "../path/to/socket", "schema", HAS_PASSWORD, HAS_NO_PORT,
               Transport_type::Socket);
  validate_uri("mysqlx://user:password@..%2Fpath%2Fto%2Fsocket/", "mysqlx",
               "user", "password", NO_HOST, NO_PORT, "../path/to/socket",
               NO_SCHEMA, HAS_PASSWORD, HAS_NO_PORT, Transport_type::Socket);
  validate_uri("mysqlx://user:password@..%2Fpath%2Fto%2Fsocket", "mysqlx",
               "user", "password", NO_HOST, NO_PORT, "../path/to/socket",
               NO_SCHEMA, HAS_PASSWORD, HAS_NO_PORT, Transport_type::Socket);

  //                0    0    1    1    2    2    3    3    4    4    5    5
  //                0    5    0    5    0    5    0    5    0    5    0    5
  validate_bad_uri("mysqlx://user:password@/path/to/socket/schema",
                   "Illegal character [/] found at position 28");
  validate_bad_uri("mysqlx://user:password@./path/to/socket/schema",
                   "Illegal character [/] found at position 24");
  validate_bad_uri("mysqlx://user:password@../path/to/socket/schema",
                   "Illegal character [/] found at position 25");
}

TEST(Uri_parser, parse_pipe) {
  //            0    0    1    1    2    2    3    3    4    4    5    5
  //            0    5    0    5    0    5    0    5    0    5    0    5
  validate_uri("mysqlx://user:password@\\.d%3A%5Cpath%5Cto%5Csocket/schema",
               "mysqlx", "user", "password", NO_HOST, NO_PORT,
               "d:\\path\\to\\socket", "schema", HAS_PASSWORD, HAS_NO_PORT,
               Transport_type::Pipe);
  validate_uri("mysqlx://user:password@\\.d%3A%5Cpath%5Cto%5Csocket/", "mysqlx",
               "user", "password", NO_HOST, NO_PORT, "d:\\path\\to\\socket",
               NO_SCHEMA, HAS_PASSWORD, HAS_NO_PORT, Transport_type::Pipe);
  validate_uri("mysqlx://user:password@\\.d%3A%5Cpath%5Cto%5Csocket", "mysqlx",
               "user", "password", NO_HOST, NO_PORT, "d:\\path\\to\\socket",
               NO_SCHEMA, HAS_PASSWORD, HAS_NO_PORT, Transport_type::Pipe);

  validate_uri("mysqlx://user:password@\\.(d:%5Cpath%5Cto%5Csocket)/schema",
               "mysqlx", "user", "password", NO_HOST, NO_PORT,
               "d:\\path\\to\\socket", "schema", HAS_PASSWORD, HAS_NO_PORT,
               Transport_type::Pipe);
  validate_uri("mysqlx://user:password@\\.(d:%5Cpath%5Cto%5Csocket)/", "mysqlx",
               "user", "password", NO_HOST, NO_PORT, "d:\\path\\to\\socket",
               NO_SCHEMA, HAS_PASSWORD, HAS_NO_PORT, Transport_type::Pipe);
  validate_uri("mysqlx://user:password@\\.(d:%5Cpath%5Cto%5Csocket)", "mysqlx",
               "user", "password", NO_HOST, NO_PORT, "d:\\path\\to\\socket",
               NO_SCHEMA, HAS_PASSWORD, HAS_NO_PORT, Transport_type::Pipe);

  validate_uri("mysqlx://user:password@\\.(d:/path/to/socket)/schema", "mysqlx",
               "user", "password", NO_HOST, NO_PORT, "d:/path/to/socket",
               "schema", HAS_PASSWORD, HAS_NO_PORT, Transport_type::Pipe);
  validate_uri("mysqlx://user:password@\\.(d:/path/to/socket)/", "mysqlx",
               "user", "password", NO_HOST, NO_PORT, "d:/path/to/socket",
               NO_SCHEMA, HAS_PASSWORD, HAS_NO_PORT, Transport_type::Pipe);
  validate_uri("mysqlx://user:password@\\.(d:/path/to/socket)", "mysqlx",
               "user", "password", NO_HOST, NO_PORT, "d:/path/to/socket",
               NO_SCHEMA, HAS_PASSWORD, HAS_NO_PORT, Transport_type::Pipe);

  //                0    0    1    1    2    2    3    3    4    4    5    5
  //                0    5    0    5    0    5    0    5    0    5    0    5
  validate_bad_uri("mysqlx://user:password@\\.d:%5Cpath%5Cto%5Csocket/schema",
                   "Illegal character [:] found at position 26");
  validate_bad_uri("mysqlx://user:password@\\.d%3A\\path%5Cto%5Csocket/schema",
                   "Illegal character [\\] found at position 29");
}

TEST(Uri_parser, parse_path) {
  validate_uri("mysqlx://user@10.150.123.45:2845/", "mysqlx", "user",
               NO_PASSWORD, "10.150.123.45", 2845, NO_SOCK, NO_DB,
               HAS_NO_PASSWORD, HAS_PORT, Transport_type::Tcp);
  validate_uri("mysqlx://user@10.150.123.45:2845/world", "mysqlx", "user",
               NO_PASSWORD, "10.150.123.45", 2845, NO_SOCK, "world",
               HAS_NO_PASSWORD, HAS_PORT, Transport_type::Tcp);
  validate_uri("mysqlx://user@10.150.123.45:2845/world@x", "mysqlx", "user",
               NO_PASSWORD, "10.150.123.45", 2845, NO_SOCK, "world@x",
               HAS_NO_PASSWORD, HAS_PORT, Transport_type::Tcp);
  validate_uri("mysqlx://user@10.150.123.45:2845/world:x", "mysqlx", "user",
               NO_PASSWORD, "10.150.123.45", 2845, NO_SOCK, "world:x",
               HAS_NO_PASSWORD, HAS_PORT, Transport_type::Tcp);
  validate_uri("mysqlx://user@10.150.123.45:2845/world%20x", "mysqlx", "user",
               NO_PASSWORD, "10.150.123.45", 2845, NO_SOCK, "world x",
               HAS_NO_PASSWORD, HAS_PORT, Transport_type::Tcp);

  //                0    0    1    1    2    2    3    3    4    4    5    5
  //                0    5    0    5    0    5    0    5    0    5    0    5
  validate_bad_uri("mysqlx://user@10.150.123.45:2845/world%x",
                   "Illegal character [%] found at position 38");
  validate_bad_uri("mysqlx://user@10.150.123.45:2845/world%2x",
                   "Illegal character [%] found at position 38");
  validate_bad_uri("mysqlx://user@10.150.123.45:2845/world%20x/subpath",
                   "Illegal character [/] found at position 32");
}

// NOTE(rennox): The URI parser is able to parse arrays and null values
// But the produced Connection_options does not allow them since they
// are not required by the current connection specs
TEST(Uri_parser, parse_query) {
  std::map<std::string, std::vector<std::string> > atts;
  atts[mysqlshdk::db::kSslCa] = {"value"};
  atts[mysqlshdk::db::kAuthMethod] = {"my space"};
  // atts[mysqlshdk::db::kSslCert] = {};
  atts[mysqlshdk::db::kSslCert] = {"cert"};
  atts[mysqlshdk::db::kSslCert] = {"cert"};
  // atts[kSslCaPath] = {"[one,two,three]"};
  atts[mysqlshdk::db::kSslCaPath] = {"ca_path"};
  atts[mysqlshdk::db::kSslCiphers] = {"/what/e/ver"};
  // validate_uri("mysqlx://user@10.150.123.45:2845/world?ssl-ca=value&auth-method=my%20space&ssl-cert&sslCapath=[one,two,three]&ssl-ciphers=(/what/e/ver)",
  // "mysqlx", "user", NO_PASSWORD, "10.150.123.45", 2845, NO_SOCK, "world",
  // HAS_NO_PASSWORD, HAS_PORT, Transport_type::Tcp, &atts);
  validate_uri(
      "mysqlx://user@10.150.123.45:2845/"
      "world?ssl-ca=value&auth-method=my%20space&ssl-cert=cert&ssl-capath=ca_path&"
      "ssl-ciphers=(/what/e/ver)",
      "mysqlx", "user", NO_PASSWORD, "10.150.123.45", 2845, NO_SOCK, "world",
      HAS_NO_PASSWORD, HAS_PORT, Transport_type::Tcp, &atts);
}
};  // namespace proj_parser_tests
};  // namespace testing
