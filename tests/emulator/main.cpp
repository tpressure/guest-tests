/* Copyright Cyberus Technology GmbH *
 *        All rights reserved        */

#include <debugport_interface.h>
#include <trace.hpp>

#include <baretest/baretest.hpp>
#include <compiler.hpp>
#include <decoder/gdt.hpp>
#include <usermode.hpp>

#include "helpers.hpp"

void prologue()
{
    // Initialize all data segments with the data selector, because in 64-bit mode,
    // that's not necessary. But we need it to test something.
    for (auto f : {mov_to_ds, mov_to_es, mov_to_fs, mov_to_gs, mov_to_ss}) {
        f(SEL_DATA, false);
    }
}

BARETEST_RUN;