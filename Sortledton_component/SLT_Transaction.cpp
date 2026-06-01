//
// Created by per on 23.12.20.
//

#include "SLT_Transaction.h"

void SLT_Transaction::use_does_not_exists_semantics() {
  use_edge_does_not_exists_semantics();
  use_vertex_does_not_exists_semantics();
}
