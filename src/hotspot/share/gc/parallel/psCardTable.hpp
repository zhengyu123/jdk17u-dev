/*
 * Copyright (c) 2001, 2019, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

#ifndef SHARE_GC_PARALLEL_PSCARDTABLE_HPP
#define SHARE_GC_PARALLEL_PSCARDTABLE_HPP

#include "gc/shared/cardTable.hpp"
#include "oops/oop.hpp"

class MutableSpace;
class ObjectStartArray;
class PSPromotionManager;

class PSCardTable: public CardTable {
  friend class PSStripeShadowCardTable;
  static constexpr size_t num_cards_in_stripe = 128;
  static_assert(num_cards_in_stripe >= 1, "progress");

  volatile int _preprocessing_active_workers;

  bool is_dirty(CardValue* card) {
    return !is_clean(card);
  }

  bool is_clean(CardValue* card) {
    return *card == clean_card_val();
  }

  // Iterate the stripes with the given index and copy imprecise card marks of
  // objects reaching into a stripe to its first card.
  template <typename Func>
  void preprocess_card_table_parallel(Func&& object_start,
                                      HeapWord* old_gen_bottom,
                                      HeapWord* old_gen_top,
                                      uint stripe_index,
                                      uint n_stripes);

  // Scavenge contents on dirty cards of the given stripe [start, end).
  template <typename Func>
  void process_range(Func&& object_start,
                     PSPromotionManager* pm,
                     HeapWord* const start,
                     HeapWord* const end);

  void verify_all_young_refs_precise_helper(MemRegion mr);

  enum ExtendedCardValue {
    youngergen_card   = CT_MR_BS_last_reserved + 1,
    verify_card       = CT_MR_BS_last_reserved + 5
  };

  void scan_obj_with_limit(PSPromotionManager* pm,
                           oop obj,
                           HeapWord* start,
                           HeapWord* end);

 public:
  PSCardTable(MemRegion whole_heap) : CardTable(whole_heap),
                                      _preprocessing_active_workers(0) {}

  static CardValue youngergen_card_val() { return youngergen_card; }
  static CardValue verify_card_val()     { return verify_card; }

  // Scavenge support
  void pre_scavenge(HeapWord* old_gen_bottom, uint active_workers);
  // Scavenge contents of stripes with the given index.
  void scavenge_contents_parallel(ObjectStartArray* start_array,
                                  HeapWord* old_gen_bottom,
                                  HeapWord* old_gen_top,
                                  PSPromotionManager* pm,
                                  uint stripe_index,
                                  uint n_stripes);

  bool addr_is_marked_imprecise(void *addr);
  bool addr_is_marked_precise(void *addr);

  void set_card_newgen(void* addr)   { CardValue* p = byte_for(addr); *p = verify_card; }

  // Testers for entries
  static bool card_is_dirty(int value)      { return value == dirty_card; }
  static bool card_is_newgen(int value)     { return value == youngergen_card; }
  static bool card_is_clean(int value)      { return value == clean_card; }
  static bool card_is_verify(int value)     { return value == verify_card; }

  // Card marking
  void inline_write_ref_field_gc(void* field) {
    CardValue* byte = byte_for(field);
    *byte = youngergen_card;
  }

  // ReduceInitialCardMarks support
  bool is_in_young(oop obj) const;

#ifdef ASSERT
  bool is_valid_card_address(CardValue* addr) {
    return (addr >= _byte_map) && (addr < _byte_map + _byte_map_size);
  }
#endif // ASSERT

  // Verification
  void verify_all_young_refs_imprecise();
  void verify_all_young_refs_precise();
};

#endif // SHARE_GC_PARALLEL_PSCARDTABLE_HPP
