#include "memtable.h"

class Sstable {
private:
  Memtable &memtable;

public:
  Sstable(Memtable &memtable);
  void write();
  void read();
};
