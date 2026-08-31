#pragma once

#include "memtable.h"
#include "sstable.h"
#include "wal.h"
#include <memory>
#include <vector>

class LsmEngine {
private:
  std::vector<std::unique_ptr<Sstable>> files;
  Memtable memtable;
  Wal wal;

public:
  get();
  put();
  delete ();
};
