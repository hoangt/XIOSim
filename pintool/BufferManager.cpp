#include "BufferManager.h"

#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <stack>
#include <sstream>

extern int num_threads;

ostream& operator<< (ostream &out, handshake_container_t &hand)
{
  out << "hand:" << " ";
  out << hand.flags.valid;
  out << hand.flags.isFirstInsn;
  out << hand.flags.isLastInsn;
  out << hand.flags.killThread;
  out << " ";
  out << hand.mem_buffer.size();
  out << " ";
  out << "pc:" << hand.handshake.pc << " ";
  out << "coreid:" << hand.handshake.coreID << " ";
  out << "npc:" << hand.handshake.npc << " ";
  out << "tpc:" << hand.handshake.tpc << " ";
  out << "brtaken:" << hand.handshake.brtaken << " ";
  out << "ins:" << hand.handshake.ins << " ";
  out << "flags:" << hand.handshake.sleep_thread << hand.handshake.resume_thread;
  out << hand.handshake.iteration_correction << hand.handshake.real;
  out << hand.handshake.in_critical_section;
  out << " slicenum:" << hand.handshake.slice_num << " ";
  out << "feederslicelen:" << hand.handshake.feeder_slice_length << " ";
  out << "feedersliceweight:" << hand.handshake.slice_weight_times_1000 << " ";
  out.flush();
  return out;
}

BufferManager::BufferManager()
{
  int pid = getpgrp();
  ostringstream iss;
  iss << pid;
  gpid_ = iss.str();
  assert(gpid_.length() > 0);

  bridgeDirs_.push_back("/dev/shm/");
  bridgeDirs_.push_back("/tmp/");
  bridgeDirs_.push_back("/group/brooks/brownell/tmp/");
  bridgeDirs_.push_back("/group/vlsiarch/brownell/tmp/");
}

BufferManager::~BufferManager()
{
  for(int i = 0; i < (int)bridgeDirs_.size(); i++) {
    string cmd = "/bin/rm -rf " + bridgeDirs_[i] + gpid_ + "_* &";
    int retVal = system(cmd.c_str());
    (void)retVal;
    assert(retVal == 0);        
  }
}

handshake_container_t* BufferManager::front(THREADID tid, bool isLocal)
{
  if(consumeBuffer_[tid]->size() > 0) {
    assert(isLocal);
    handshake_container_t* returnVal = consumeBuffer_[tid]->front();
    return returnVal;
  }

  assert(!isLocal);

  GetLock(locks_[tid], tid+1);

  assert(queueSizes_[tid] > 0);

  assert(fileEntryCount_[tid] > 0 || produceBuffer_[tid]->size() > 0);

  if(fileEntryCount_[tid] > 0) {
    copyFileToConsumer(tid);
    assert(!consumeBuffer_[tid]->empty());
    handshake_container_t* returnVal = consumeBuffer_[tid]->front();
    ReleaseLock(locks_[tid]);
    return returnVal;
  }
  assert(fileEntryCount_[tid] == 0);
  assert(consumeBuffer_[tid]->empty());

  int spins = 0;
  while(consumeBuffer_[tid]->empty()) {
    ReleaseLock(locks_[tid]);
    ReleaseLock(&simbuffer_lock);
    PIN_Yield();    
    GetLock(&simbuffer_lock, tid+1);
    GetLock(locks_[tid], tid+1);
    spins++;
    if(spins >= 2) { // DONT CHANGE THIS MUST EQUAL 3
      spins = 0;
      if(fileEntryCount_[tid] == 0) {
	continue;
      }
      copyFileToConsumer(tid);
    }
  }

  assert(consumeBuffer_[tid]->size() > 0);
  assert(consumeBuffer_[tid]->front()->flags.valid);
  assert(queueSizes_[tid] > 0);
  handshake_container_t* resultVal = consumeBuffer_[tid]->front();
  ReleaseLock(locks_[tid]);
  return resultVal;
}

handshake_container_t* BufferManager::back(THREADID tid)
{
  GetLock(locks_[tid], tid+1);
  assert(queueSizes_[tid] > 0);
  handshake_container_t* returnVal = produceBuffer_[tid]->back();
  ReleaseLock(locks_[tid]);
  return returnVal;
}

bool BufferManager::empty(THREADID tid)
{
  GetLock(locks_[tid], tid+1);
  bool result = queueSizes_[tid] == 0;
  if(result) {
    resetPool(tid);
  }
  ReleaseLock(locks_[tid]);
  return result;
}

handshake_container_t* BufferManager::get_buffer(THREADID tid)
{
  GetLock(locks_[tid], tid+1);
  // Push is guaranteed to succeed because each call to
  // this->get_buffer() is followed by a call to this->producer_done()
  // which will make space if full
  handshake_container_t* result = produceBuffer_[tid]->get_buffer();
  produceBuffer_[tid]->push_done();
  queueSizes_[tid]++;
  assert(pool_[tid] > 0);
  pool_[tid]--;

  ReleaseLock(locks_[tid]);
  return result;
}

void BufferManager::producer_done(THREADID tid, bool keepLock)
{
  GetLock(locks_[tid], tid+1);

  ASSERTX(!produceBuffer_[tid]->empty());
  handshake_container_t* last = produceBuffer_[tid]->back();
  ASSERTX(last->flags.valid);
  
  if(!keepLock) {
    reserveHandshake(tid);
    ReleaseLock(&simbuffer_lock);
  }
  else {
    pool_[tid]++;
  }

  if(produceBuffer_[tid]->full() || ( (consumeBuffer_[tid]->size() == 0) && (fileEntryCount_[tid] == 0))) {    
#ifdef DEBUG  
    int produceSize = produceBuffer_[tid]->size();
#endif
    bool checkSpace = !keepLock;
    copyProducerToFile(tid, checkSpace);
    assert(fileEntryCount_[tid] > 0);
    assert(fileEntryCount_[tid] >= produceSize);
    assert(produceBuffer_[tid]->size() == 0);
  }
  
  assert(!produceBuffer_[tid]->full());
  
  ReleaseLock(locks_[tid]);

  
  if(!keepLock) {
    GetLock(&simbuffer_lock, tid+1);
  }
}

void BufferManager::flushBuffers(THREADID tid)
{
  GetLock(locks_[tid], tid+1);
  map<THREADID, string>::iterator it;

  if(produceBuffer_[tid]->size() > 0) {
    copyProducerToFile(tid, false);
  }
  ReleaseLock(locks_[tid]);
}

int BufferManager::getConsumerSize(THREADID tid)
{
  return consumeBuffer_[tid]->size();
}

void BufferManager::applyConsumerChanges(THREADID tid, int numChanged)
{
  GetLock(locks_[tid], tid+1);
  
  pool_[tid] += numChanged;

  assert(queueSizes_[tid] >= numChanged);
  queueSizes_[tid] -= numChanged;

  ReleaseLock(locks_[tid]);
}

void BufferManager::pop(THREADID tid)
{
  popped_ = true;
  assert(consumeBuffer_[tid]->size() > 0);
  consumeBuffer_[tid]->pop();
}

bool BufferManager::hasThread(THREADID tid)
{
  bool result = queueSizes_.count(tid);
  return (result != 0);
}

unsigned int BufferManager::size()
{
  unsigned int result = queueSizes_.size();
  return result;
}

unsigned int BufferManager::size(THREADID tid)
{
  GetLock(locks_[tid], tid+1);
  unsigned int result = queueSizes_[tid];
  ReleaseLock(locks_[tid]);
  return result;
}


void BufferManager::reserveHandshake(THREADID tid)
{
  bool popped_ = false;

  int queueLimit;
  if(num_threads > 1) {
    queueLimit = 100000001;
  }
  else {
    queueLimit = 100001;
  }
  
  while(pool_[tid] == 0) {
    assert(queueSizes_[tid] > 0);
    ReleaseLock(locks_[tid]);
    ReleaseLock(&simbuffer_lock);

    popped_ = false;
    PIN_Sleep(3000);
    
    GetLock(&simbuffer_lock, tid+1);
    GetLock(locks_[tid], tid+1);

    if(popped_) {
      continue;
    }

    if(num_threads == 1 || (!useRealFile_)) {
      continue;
    }	
    
    if(queueSizes_[tid] < queueLimit) {
      pool_[tid] += 50000;
      cerr << tid << " [reserveHandshake()]: Increasing file up to " << queueSizes_[tid] + pool_[tid] << endl;
      break;
    }
    cerr << tid << " [reserveHandshake()]: File size too big to expand, abort():" << queueSizes_[tid] << endl;
    this->abort();
  }
}

void BufferManager::copyProducerToFile(THREADID tid, bool checkSpace)
{
  if(useRealFile_) {
    copyProducerToFileReal(tid, checkSpace);
  }
  else {
    copyProducerToFileFake(tid);
  }
  sync();
}

void BufferManager::copyFileToConsumer(THREADID tid)
{
  if(useRealFile_) {
    copyFileToConsumerReal(tid);
  }
  else {
    copyFileToConsumerFake(tid);
  }
  sync();
}

void BufferManager::copyProducerToFileFake(THREADID tid)
{
  while(produceBuffer_[tid]->size() > 0) {    
    handshake_container_t* handshake = produceBuffer_[tid]->front();
    handshake_container_t* handfake = fakeFile_[tid]->get_buffer();
    handshake->CopyTo(handfake);
    fakeFile_[tid]->push_done();

    produceBuffer_[tid]->pop(); 	
    fileEntryCount_[tid]++;
  }
}


void BufferManager::copyFileToConsumerFake(THREADID tid)
{
  while(fakeFile_[tid]->size() > 0) {
    if(consumeBuffer_[tid]->full()) {
      break;
    }

    handshake_container_t* handshake = consumeBuffer_[tid]->get_buffer();
    fakeFile_[tid]->front()->CopyTo(handshake);
    consumeBuffer_[tid]->push_done();
    fakeFile_[tid]->pop();
    fileEntryCount_[tid]--;
  }
}


void BufferManager::copyProducerToFileReal(THREADID tid, bool checkSpace)
{
  int result;  
  bool madeFile = false;
  if(checkSpace) {
    for(int i = 0; i < (int)bridgeDirs_.size(); i++) {
      int space = getKBFreeSpace(bridgeDirs_[i]);
      if(space > 1000000) {
	fileNames_[tid].push_back(genFileName(bridgeDirs_[i]));    
	madeFile = true;
	break;
      }
      cerr << "Out of space on " + bridgeDirs_[i] + " !!!" << endl;
    }
    if(madeFile == false) {
      cerr << "Nowhere left for the poor file bridge :(" << endl;
      this->abort();
    }
  }
  else {
    fileNames_[tid].push_back(genFileName(bridgeDirs_[0]));    
  }
  
  fileCounts_[tid].push_back(0);

  int fd = open(fileNames_[tid].back().c_str(), O_WRONLY | O_CREAT, 0777);
  if(fd == -1) {
    cerr << "Opened to write: " << fileNames_[tid].back();
    cerr << "Pipe open error: " << fd << " Errcode:" << strerror(errno) << endl;
    this->abort();
  }

  while(!produceBuffer_[tid]->empty()) {
    writeHandshake(tid, fd, produceBuffer_[tid]->front());
    produceBuffer_[tid]->pop();
    fileCounts_[tid].back() += 1;
    fileEntryCount_[tid]++;
  }

  sync();

  result = close(fd);
  if(result == -1) {
    cerr << "Close error: " << " Errcode:" << strerror(errno) << endl;
    this->abort();
  }

  sync();

  assert(produceBuffer_[tid]->size() == 0);
  assert(fileEntryCount_[tid] >= 0);
}


void BufferManager::copyFileToConsumerReal(THREADID tid)
{
  int result;

  int fd = open(fileNames_[tid].front().c_str(), O_RDWR);
  if(fd == -1) {
    cerr << "Opened to read: " << fileNames_[tid].front();
    cerr << "Pipe open error: " << fd << " Errcode:" << strerror(errno) << endl;
    this->abort();
  }

  int count = 0;
  bool validRead = true;
  while(fileCounts_[tid].front() > 0) {
    assert(!consumeBuffer_[tid]->full());

    handshake_container_t* handshake = consumeBuffer_[tid]->get_buffer();
    validRead = readHandshake(tid, fd, handshake);
    assert(validRead);
    consumeBuffer_[tid]->push_done();
    count++;
    fileEntryCount_[tid]--;
    fileCounts_[tid].front() -= 1;
  }

  result = close(fd);
  if(result == -1) {
    cerr << "Close error: " << " Errcode:" << strerror(errno) << endl;
    this->abort();
  }

  assert(fileCounts_[tid].front() == 0);
  remove(fileNames_[tid].front().c_str());
  fileNames_[tid].pop_front();
  fileCounts_[tid].pop_front();

  sync();
  assert(fileEntryCount_[tid] >= 0);
}

void  BufferManager::writeHandshake(THREADID tid, int fd, handshake_container_t* handshake)
{
  int mapSize = handshake->mem_buffer.size();
  const int handshakeBytes = sizeof(P2Z_HANDSHAKE);
  const int flagBytes = sizeof(handshake_flags_t);
  const int mapEntryBytes = sizeof(UINT32) + sizeof(UINT8);
  int mapBytes = mapSize * mapEntryBytes;
  int totalBytes = sizeof(int) + handshakeBytes + flagBytes + mapBytes;

  void * writeBuffer = writeBuffer_[tid];
  void * buffPosition = writeBuffer;

  memcpy((char*)buffPosition, &(mapSize), sizeof(int));
  buffPosition = (char*)buffPosition + sizeof(int);

  memcpy((char*)buffPosition, &(handshake->handshake), handshakeBytes);
  buffPosition = (char*)buffPosition + handshakeBytes;

  memcpy((char*)buffPosition, &(handshake->flags), flagBytes);
  buffPosition = (char*)buffPosition + flagBytes;

  map<UINT32, UINT8>::iterator it;
  for(it = handshake->mem_buffer.begin(); it != handshake->mem_buffer.end(); it++) {
    memcpy((char*)buffPosition, &(it->first), sizeof(UINT32));
    buffPosition = (char*)buffPosition + sizeof(UINT32);

    memcpy((char*)buffPosition, &(it->second), sizeof(UINT8));
    buffPosition = (char*)buffPosition + sizeof(UINT8);
  }

  assert(((unsigned long long int)writeBuffer) + totalBytes == ((unsigned long long int)buffPosition));

  int bytesWritten = write(fd, writeBuffer, totalBytes);
  if(bytesWritten == -1) {
    cerr << "Pipe write error: " << bytesWritten << " Errcode:" << strerror(errno) << endl;
    this->abort();
  }
  if(bytesWritten != totalBytes) {
    cerr << "File write error: " << bytesWritten << " expected:" << totalBytes << endl;
    this->abort();
  }
}

bool BufferManager::readHandshake(THREADID tid, int fd, handshake_container_t* handshake)
{
  const int handshakeBytes = sizeof(P2Z_HANDSHAKE);
  const int flagBytes = sizeof(handshake_flags_t);
  const int mapEntryBytes = sizeof(UINT32) + sizeof(UINT8);

  int mapSize;
  int bytesRead = read(fd, &(mapSize), sizeof(int));
  if(bytesRead == -1) {
    cerr << "File read error: " << bytesRead << " Errcode:" << strerror(errno) << endl;
    this->abort();
  } 
  assert(bytesRead == sizeof(int));

  int mapBytes = mapSize * mapEntryBytes;
  int totalBytes = handshakeBytes + flagBytes + mapBytes;

  void * readBuffer = readBuffer_[tid];
  assert(readBuffer != NULL);

  bytesRead = read(fd, readBuffer, totalBytes);
  if(bytesRead == -1) {
    cerr << "File read error: " << bytesRead << " Errcode:" << strerror(errno) << endl;
    this->abort();
  }
  assert(bytesRead == totalBytes);

  void * buffPosition = readBuffer;
  memcpy(&(handshake->handshake), buffPosition, handshakeBytes);
  buffPosition = (char*)buffPosition + handshakeBytes;

  memcpy(&(handshake->flags), buffPosition, flagBytes);
  buffPosition = (char*)buffPosition + flagBytes;

  handshake->mem_buffer.clear();
  for(int i = 0; i < mapSize; i++) {
    UINT32 first;
    UINT8 second;

    first = *((UINT32*)buffPosition);
    buffPosition = (char*)buffPosition + sizeof(UINT32);

    second = *((UINT8*)buffPosition);
    buffPosition = (char*)buffPosition + sizeof(UINT8);

    (handshake->mem_buffer)[first] = second;
  }

  assert(((unsigned long long int)readBuffer) + totalBytes == ((unsigned long long int)buffPosition));

  return true;
}

void BufferManager::abort(){
  this->signalCallback(SIGABRT);
  exit(1);
  abort();
}

void BufferManager::signalCallback(int signum)
{
  cerr << "BufferManager caught signal:" << signum << endl;
  map<THREADID, string>::iterator it;

  for(int i = 0; i < (int)bridgeDirs_.size(); i++) {
    string cmd = "/bin/rm -rf " + bridgeDirs_[i] + gpid_ + "_* &";
    int retVal = system(cmd.c_str());
    (void)retVal;
    assert(retVal == 0);        
  }
}

void BufferManager::allocateThread(THREADID tid) 
{
  assert(queueSizes_.count(tid) == 0);
  
  queueSizes_[tid] = 0;
  fileEntryCount_[tid] = 0;
  if(num_threads > 1) {
    useRealFile_ = true;
  }
  else {
    useRealFile_ = false;
  }

  //  useRealFile_ = false;

  int bufferEntries = 640000 / 2;
  int bufferCapacity = bufferEntries / 2 / num_threads;

  if(!useRealFile_) {
    bufferCapacity /= 8;
    fakeFile_[tid] = new Buffer(120000);
  }
  
  consumeBuffer_[tid] = new Buffer(bufferCapacity);
  produceBuffer_[tid] = new Buffer(bufferCapacity);
  assert(produceBuffer_[tid]->capacity() <= consumeBuffer_[tid]->capacity());
  resetPool(tid);
  produceBuffer_[tid]->get_buffer()->flags.isFirstInsn = true;
  locks_[tid] = new PIN_LOCK();
  InitLock(locks_[tid]);
    
  cerr << tid << " Creating temp files with prefix "  << gpid_ << "_*" << endl;
  // Start with one page read buffer
  readBufferSize_[tid] = 4096;
  readBuffer_[tid] = malloc(4096);
  assert(readBuffer_[tid]);

  writeBufferSize_[tid] = 4096;
  writeBuffer_[tid] = malloc(4096);
  assert(writeBuffer_[tid]);
}

string BufferManager::genFileName(string path)
{
  string temp = tempnam(path.c_str(), gpid_.c_str());
  temp.insert(8 + gpid_.length() + 1, "_");
  return temp;
}

void BufferManager::resetPool(THREADID tid) 
{
  int poolFactor = 1;
  if(num_threads > 1) {
    poolFactor = 6;
  }
  pool_[tid] = (consumeBuffer_[tid]->capacity() + produceBuffer_[tid]->capacity()) * poolFactor;
}

int BufferManager::getKBFreeSpace(string path) 
{
  string cmd = "/bin/df " + path + " | awk '{print $4}' | tail -1";
  FILE* availSpace = popen(cmd.c_str(), "r");
  char buffer[1024];
  char *line_p = fgets(buffer, sizeof(buffer), availSpace);
  pclose(availSpace);
  return atoi(line_p);
}
