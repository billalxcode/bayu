// SPDX-License-Identifier: BSD-3-Clause

#include <gtest/gtest.h>

#include "pagetest.h"
#include "pages/printers/printers.h"

TEST(PrintersPage, Opens)
{
    PageFixture page("printers");
    EXPECT_NE(page.dump().find("\"page:printers\""), std::string::npos);
}

TEST(PrintersPage, MakesTheUriOfAnAddress)
{
    using settings::printers::uriForAddress;
    EXPECT_EQ(uriForAddress("192.168.1.20"), "ipp://192.168.1.20/ipp/print");
    EXPECT_EQ(uriForAddress(" printer.local "), "ipp://printer.local/ipp/print");
    EXPECT_EQ(uriForAddress("ipps://10.0.0.5:631/ipp/print"), "ipps://10.0.0.5:631/ipp/print");
    EXPECT_EQ(uriForAddress("  "), "");
}

TEST(PrintersPage, MakesQueueNames)
{
    using settings::printers::queueName;
    EXPECT_EQ(queueName("HP LaserJet M110w (USB)"), "HP_LaserJet_M110w_USB");
    EXPECT_EQ(queueName("192.168.1.20"), "192_168_1_20");
    EXPECT_EQ(queueName("printer-2_b"), "printer-2_b");
    EXPECT_EQ(queueName("///"), "Printer");
}
