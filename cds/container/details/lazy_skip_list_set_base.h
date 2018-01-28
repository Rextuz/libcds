#ifndef CDSLIB_LAZY_SKIP_LIST_BASE_H
#define CDSLIB_LAZY_SKIP_LIST_BASE_H

#include <cds/intrusive/details/base.h>
#include <cds/details/marked_ptr.h>
#include <cds/os/timer.h>
#include <cds/gc/dhp.h>

namespace cds { namespace container {

    namespace skip_list {

        static size_t const c_nMaxHeight = 32;

        template <
            typename GC,
            typename T,
            typename Lock = std::recursive_mutex
        >
        class node
        {
        public:
            typedef cds::gc::DHP      gc;
            typedef T   value_type;
            typedef Lock lock_type;

            typedef std::size_t key_type;

            typedef node* node_ptr;
            typedef std::numeric_limits<key_type> limits;
            typedef cds::details::Allocator<node> node_allocator;

            typedef cds::details::marked_ptr<node, 1> marked_ptr;
            typedef typename gc::template atomic_marked_ptr<marked_ptr> atomic_marked_ptr;

            typedef cds::details::Allocator<atomic_marked_ptr> tower_allocator;

        protected:
            value_type value;
            key_type key;
            unsigned int m_nHeight;
            atomic_marked_ptr * m_arrNext;
            atomics::atomic<bool> _marked;
            atomics::atomic<bool> fullyLinked;

            key_type hash() {
                return std::hash<value_type>{}(value);
            }

        public:
            lock_type lock;

            node() : _marked(false), fullyLinked(false) {

            }

            ~node(){
                dispose_tower(this);
            }

            bool marked() {
                return _marked;
            }

            bool mark() {
                _marked = true;
            }

            bool mark_hard() {
                marked_ptr next_marked(next(0).load(atomics::memory_order_relaxed).ptr());
                next(0).compare_exchange_strong(next_marked, next_marked | 1, atomics::memory_order_release, atomics::memory_order_acquire);
            }

            bool fully_linked() {
                return fullyLinked.load(atomics::memory_order_relaxed);
            }

            void set_fully_linked(bool value) {
                fullyLinked.store(value, atomics::memory_order_relaxed);
            }

            key_type node_key() {
                return key;
            }

            atomic_marked_ptr& next(unsigned int nLevel) {
                return m_arrNext[nLevel];
            }

            unsigned int height() {
                return m_nHeight;
            }

            void allocate_tower(int nHeight) {
                tower_allocator ta;
                m_arrNext = ta.NewArray(nHeight, nullptr);
            }

            static void dispose_node(node * pNode) {
                node_allocator allocator;
                allocator.Delete(pNode);
            }

            static void dispose_tower(node * pNode) {
                unsigned int topLayer = pNode->height();
                tower_allocator ta;
                if (topLayer > 0)
                    ta.Delete(pNode->release_tower(), topLayer + 1);
            }

            atomic_marked_ptr * release_tower() {
                atomic_marked_ptr * pTower = m_arrNext;
                m_arrNext = nullptr;
                m_nHeight = 0;

                return pTower;
            }

            static node_ptr allocate_node(key_type key) {
                node_allocator alloc;
                node_ptr new_node = alloc.New();
                new_node->key = key;
                new_node->m_nHeight = cds::container::skip_list::c_nMaxHeight;
                new_node->allocate_tower(new_node->m_nHeight);
                new_node->fullyLinked = false;

                return new_node;
            }

            static node_ptr allocate_node(value_type v, unsigned int topLayer) {
                node_allocator alloc;
                node_ptr new_node = alloc.New();

                new_node->value = v;
                new_node->key = new_node->hash();
                new_node->m_nHeight = topLayer;
                new_node->fullyLinked = false;

                new_node->allocate_tower(topLayer + 1);

                return new_node;
            }

            static node * min_key() {
                node_ptr new_node = allocate_node(limits::min());

                return new_node;
            }

            static node * max_key() {
                node_ptr new_node = allocate_node(limits::max() / 1000);

                return new_node;
            }

        };

        /// Turbo-pascal random level generator
        /**
            This uses a cheap pseudo-random function that was used in Turbo Pascal.

            The random generator should return numbers from range [0..31].

            From Doug Lea's ConcurrentSkipListMap.java.
        */
        template <unsigned MaxHeight>
        class turbo
        {
            //@cond
            atomics::atomic<unsigned int>    m_nSeed;

            static_assert( MaxHeight > 1, "MaxHeight" );
            static_assert( MaxHeight <= c_nMaxHeight, "MaxHeight is too large" );
            static unsigned int const c_nBitMask = (1u << ( MaxHeight - 1 )) - 1;
            //@endcond
        public:
            /// The upper bound of generator's return value. The generator produces random number in range <tt>[0..c_nUpperBound)</tt>
            static unsigned int const c_nUpperBound = MaxHeight;

            /// Initializes the generator instance
            turbo()
            {
                m_nSeed.store( (unsigned int) cds::OS::Timer::random_seed(), atomics::memory_order_relaxed );
            }

            /// Main generator function
            unsigned int operator()()
            {
                /*
                private int randomLevel() {
                    int level = 0;
                    int r = randomSeed;
                    randomSeed = r * 134775813 + 1;
                    if (r < 0) {
                        while ((r <<= 1) > 0)
                            ++level;
                    }
                return level;
                }
                */
                /*
                    The low bits are apparently not very random (the original used only
                    upper 16 bits) so we traverse from highest bit down (i.e., test
                    sign), thus hardly ever use lower bits.
                */
                unsigned int x = m_nSeed.load( atomics::memory_order_relaxed ) * 134775813 + 1;
                m_nSeed.store( x, atomics::memory_order_relaxed );
                unsigned int nLevel = ( x & 0x80000000 ) ? ( c_nUpperBound - 1 - cds::bitop::MSBnz( (x & c_nBitMask ) | 1 )) : 0;

                assert( nLevel < c_nUpperBound );
                return nLevel;
            }
        };
        /// Turbo-Pascal random level generator, max height 32
        typedef turbo<c_nMaxHeight> turbo32;

        template <typename EventCounter = cds::atomicity::event_counter>
        struct stat {
            typedef EventCounter event_counter ; ///< Event counter type

            event_counter   m_nNodeHeightAdd[c_nMaxHeight] ; ///< Count of added node of each height
            event_counter   m_nNodeHeightDel[c_nMaxHeight] ; ///< Count of deleted node of each height
            event_counter   m_nInsertSuccess        ; ///< Count of success insertion
            event_counter   m_nInsertFailed         ; ///< Count of failed insertion
            event_counter   m_nInsertRetries        ; ///< Count of unsuccessful retries of insertion
            event_counter   m_nUpdateExist          ; ///< Count of \p update() call for existed node
            event_counter   m_nUpdateNew            ; ///< Count of \p update() call for new node
            event_counter   m_nUnlinkSuccess        ; ///< Count of successful call of \p unlink
            event_counter   m_nUnlinkFailed         ; ///< Count of failed call of \p unlink
            event_counter   m_nEraseSuccess         ; ///< Count of successful call of \p erase
            event_counter   m_nEraseFailed          ; ///< Count of failed call of \p erase
            event_counter   m_nEraseRetry           ; ///< Count of retries while erasing node
            event_counter   m_nFindFastSuccess      ; ///< Count of successful call of \p find and all derivatives (via fast-path)
            event_counter   m_nFindFastFailed       ; ///< Count of failed call of \p find and all derivatives (via fast-path)
            event_counter   m_nFindSlowSuccess      ; ///< Count of successful call of \p find and all derivatives (via slow-path)
            event_counter   m_nFindSlowFailed       ; ///< Count of failed call of \p find and all derivatives (via slow-path)
            event_counter   m_nRenewInsertPosition  ; ///< Count of renewing position events while inserting
            event_counter   m_nLogicDeleteWhileInsert; ///< Count of events "The node has been logically deleted while inserting"
            event_counter   m_nRemoveWhileInsert    ; ///< Count of evnts "The node is removing while inserting"
            event_counter   m_nFastErase            ; ///< Fast erase event counter
            event_counter   m_nFastExtract          ; ///< Fast extract event counter
            event_counter   m_nSlowErase            ; ///< Slow erase event counter
            event_counter   m_nSlowExtract          ; ///< Slow extract event counter
            event_counter   m_nExtractSuccess       ; ///< Count of successful call of \p extract
            event_counter   m_nExtractFailed        ; ///< Count of failed call of \p extract
            event_counter   m_nExtractRetries       ; ///< Count of retries of \p extract call
            event_counter   m_nExtractMinSuccess    ; ///< Count of successful call of \p extract_min
            event_counter   m_nExtractMinFailed     ; ///< Count of failed call of \p extract_min
            event_counter   m_nExtractMinRetries    ; ///< Count of retries of \p extract_min call
            event_counter   m_nExtractMaxSuccess    ; ///< Count of successful call of \p extract_max
            event_counter   m_nExtractMaxFailed     ; ///< Count of failed call of \p extract_max
            event_counter   m_nExtractMaxRetries    ; ///< Count of retries of \p extract_max call
            event_counter   m_nEraseWhileFind       ; ///< Count of erased item while searching
            event_counter   m_nExtractWhileFind     ; ///< Count of extracted item while searching (RCU only)
            event_counter   m_nMarkFailed           ; ///< Count of failed node marking (logical deletion mark)
            event_counter   m_nEraseContention      ; ///< Count of key erasing contention encountered

            //@cond
            void onAddNode( unsigned int nHeight )
            {
                assert( nHeight > 0 && nHeight <= sizeof(m_nNodeHeightAdd) / sizeof(m_nNodeHeightAdd[0]));
                ++m_nNodeHeightAdd[nHeight - 1];
            }
            void onRemoveNode( unsigned int nHeight )
            {
                assert( nHeight > 0 && nHeight <= sizeof(m_nNodeHeightDel) / sizeof(m_nNodeHeightDel[0]));
                ++m_nNodeHeightDel[nHeight - 1];
            }

            void onInsertSuccess()          { ++m_nInsertSuccess    ; }
            void onInsertFailed()           { ++m_nInsertFailed     ; }
            void onInsertRetry()            { ++m_nInsertRetries    ; }
            void onUpdateExist()            { ++m_nUpdateExist      ; }
            void onUpdateNew()              { ++m_nUpdateNew        ; }
            void onUnlinkSuccess()          { ++m_nUnlinkSuccess    ; }
            void onUnlinkFailed()           { ++m_nUnlinkFailed     ; }
            void onEraseSuccess()           { ++m_nEraseSuccess     ; }
            void onEraseFailed()            { ++m_nEraseFailed      ; }
            void onEraseRetry()             { ++m_nEraseRetry; }
            void onFindFastSuccess()        { ++m_nFindFastSuccess  ; }
            void onFindFastFailed()         { ++m_nFindFastFailed   ; }
            void onFindSlowSuccess()        { ++m_nFindSlowSuccess  ; }
            void onFindSlowFailed()         { ++m_nFindSlowFailed   ; }
            void onEraseWhileFind()         { ++m_nEraseWhileFind   ; }
            void onExtractWhileFind()       { ++m_nExtractWhileFind ; }
            void onRenewInsertPosition()    { ++m_nRenewInsertPosition; }
            void onLogicDeleteWhileInsert() { ++m_nLogicDeleteWhileInsert; }
            void onRemoveWhileInsert()      { ++m_nRemoveWhileInsert; }
            void onFastErase()              { ++m_nFastErase;         }
            void onFastExtract()            { ++m_nFastExtract;       }
            void onSlowErase()              { ++m_nSlowErase;         }
            void onSlowExtract()            { ++m_nSlowExtract;       }
            void onExtractSuccess()         { ++m_nExtractSuccess;    }
            void onExtractFailed()          { ++m_nExtractFailed;     }
            void onExtractRetry()           { ++m_nExtractRetries;    }
            void onExtractMinSuccess()      { ++m_nExtractMinSuccess; }
            void onExtractMinFailed()       { ++m_nExtractMinFailed;  }
            void onExtractMinRetry()        { ++m_nExtractMinRetries; }
            void onExtractMaxSuccess()      { ++m_nExtractMaxSuccess; }
            void onExtractMaxFailed()       { ++m_nExtractMaxFailed;  }
            void onExtractMaxRetry()        { ++m_nExtractMaxRetries; }
            void onMarkFailed()             { ++m_nMarkFailed;        }
            void onEraseContention()        { ++m_nEraseContention;   }
            //@endcond
        };

        // typedef cds::intrusive::skip_list::traits traits;
        struct traits {
            typedef opt::v::relaxed_ordering        memory_model;
            typedef turbo32                         random_level_generator;
        };

        template <typename... Options>
        struct make_traits {
#   ifdef CDS_DOXYGEN_INVOKED
            typedef implementation_defined type ;   ///< Metafunction result
#   else
            typedef typename cds::opt::make_options<
                    typename cds::opt::find_type_traits< traits, Options... >::type
                    ,Options...
            >::type   type;
#   endif
        };

        namespace details {
            template <class GC, typename T>
            class iterator {
                typedef node<GC, T> node_type;

                node_type * m_pNode;
            };
        }

    }   // namespace skip_list

}}

#endif //CDSLIB_LAZY_SKIP_LIST_BASE_H
