/*
    This file is a part of libcds - Concurrent Data Structures library

    (C) Copyright Maxim Khizhinsky (libcds.dev@gmail.com) 2006-2017

    Source code repo: http://github.com/khizmax/libcds/
    Download: http://sourceforge.net/projects/libcds/files/

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this
      list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
    SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
    CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
    OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef CDSUNIT_SET_TYPE_SKIP_LIST_H
#define CDSUNIT_SET_TYPE_SKIP_LIST_H

#include "set_type.h"

#include <cds/container/lazy_skip_list_set_dhp.h>

#include <cds_test/stat_skiplist_out.h>
#include <cds/gc/dhp.h>

namespace set {

    template <typename GC, typename T, typename Traits = cc::lazy_skip_list_set::traits >
    class SkipListSet : public cc::LazySkipListSet<GC, T, Traits>
    {
        typedef cc::LazySkipListSet<GC, T, Traits> base_class;
    public:
        template <typename Config>
        SkipListSet( Config const& /*cfg*/ )
        {}

        // for testing
        static constexpr bool const c_bExtractSupported = false;
        static constexpr bool const c_bLoadFactorDepended = false;
        static constexpr bool const c_bEraseExactKey = false;
    };

    struct tag_SkipListSet;

    template <typename Key, typename Val>
    struct set_type< tag_SkipListSet, Key, Val >: public set_type_base< Key, Val >
    {
        typedef set_type_base< Key, Val > base_class;
        typedef typename base_class::key_val key_val;
        typedef typename base_class::compare compare;
        typedef typename base_class::less less;
        typedef typename base_class::hash hash;

        class traits_LazySkipListSet_turbo32: public cds::container::lazy_skip_list_set::traits
        {};

        typedef SkipListSet< cds::gc::DHP, key_val, traits_LazySkipListSet_turbo32 > LazySkipListSet_default_turbo32;
    };

    template <typename GC, typename T, typename Traits>
    static inline void print_stat( cds_test::property_stream& o, SkipListSet<GC, T, Traits> const& s )
    {
        // o << s.statistics();
    }

} // namespace set

#define CDSSTRESS_SkipListSet_case( fixture, test_case, skiplist_set_type, key_type, value_type ) \
    TEST_F( fixture, skiplist_set_type ) \
    { \
        typedef set::set_type< tag_SkipListSet, key_type, value_type >::skiplist_set_type set_type; \
        test_case<set_type>(); \
    }

#define CDSSTRESS_SkipListSet_HP( fixture, test_case, key_type, value_type ) \
    CDSSTRESS_SkipListSet_case( fixture, test_case, LazySkipListSet_default_turbo32,             key_type, value_type ) \

#define CDSSTRESS_SkipListSet( fixture, test_case, key_type, value_type ) \
    CDSSTRESS_SkipListSet_HP( fixture, test_case, key_type, value_type ) \

#endif // #ifndef CDSUNIT_SET_TYPE_SKIP_LIST_H
