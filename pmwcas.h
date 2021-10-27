#include <stdio.h>
#include <string.h>
#include <libpmemobj.h>

/*
  note: for this implementation, we didn't implement management of memory
  pointed to by words in the pmwcas operation
  these can be manually managed by the caller (or we can add it, idk)
*/

namespace pmwcas {
  class Descriptor {
    // descriptions from arulraj paper
    public:
      // AllocateDescriptor(callback = default):
      // Allocate a
      // descriptor that will be used throughout the PMwCAS operation.
      // The user can provide a custom callback function for recycling
      // memory pointed to by the words in the PMwCAS operation.
      // Note: callback not implemented, see note
      static Descriptor AllocateDescriptor(PMEMobjpool *pop);

      // Descriptor::AddWord(address, expected, desired):
      // Specify a word to be modified. The caller provides the address
      // of the word, the expected value and the desired value.
      int AddWord(size_t address, int expected, int desired);

      // Descriptor::ReserveEntry(addr, expected, policy):
      // Similar to AddWord except the new value is left unspecified;
      // returns a pointer to the new_value field so it can be filled in
      // later. Memory referenced by old_value/new_value will be
      // recycled according to the specified policy.
      // Note: policy not implemented, see note
      int *ReserveEntry(size_t address, int expected);

      // Descriptor::RemoveWord(address):
      // Remove the word previously specified as part of the PMwCAS.
      // note: if it was allocated with ReserveEntry, do not reuse the pointer
      int RemoveWord(size_t address);

      // Descriptor::Execute():
      // Execute the PMwCAS and return true if succeeded.
      // note: paper specified this and Discard as a static method, but, why? no need
      bool Execute();

      // Descriptor::Discard():
      // Cancel the PMwCAS (only valid before
      // calling PMwCAS). No specified word will be modified.
      bool Discard();

    private:
      Descriptor();

      // object pool for pmwcas
      PMEMobjpool *pop;

      // words before execute are on heap in an std vector
      struct DescriptorWord {
        size_t address,
        int expected,
        int desired
      };
      std::vector<DescriptorWord> words;
  };
} // namespace pmwcas
