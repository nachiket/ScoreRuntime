
include Makefile.common

###---------------files used for the schedulers----------
SCHED_TREE = scheduler
SCHED_COMMON_SRCS = 
SCHED_COMMON_HEADERS = $(SCHED_TREE)/common/

SCHEDULER_SRC_TREE = $(SCHED_TREE)/ScoreSchedulerDynamic
SCHEDULER_SRCS= $(SCHED_TREE)/ScoreSchedulerDynamic/ScoreSchedulerDynamic.cc \
	        $(SCHED_TREE)/ScoreSchedulerDynamic/ScoreSchedTemplate.cc \
		$(SCHED_TREE)/ScoreSchedulerDynamic/ScoreSchedulerDynamic_util.cc \
		$(SCHED_COMMON_SRCS)
SCHEDULER_INCLUDES = -I$(SCHED_TREE)/ScoreSchedulerDynamic -I$(SCHED_COMMON_HEADERS)
###------------------------------------------------------
       #ScoreThreadCounter.cc \

SRCS = ScoreArray.cc \
       ScoreArrayPhysicalStatus.cc \
       ScoreCInterface.cc \
       ScoreCluster.cc \
       ScoreConfig.cc \
       ScoreDummyDonePage.cc \
       ScoreGraphNode.cc \
       ScoreHardwareAPI.cc \
       ScoreOperator.cc \
       ScoreOperatorElement.cc \
       ScoreOperatorInstance.cc \
       ScoreOperatorInstanceElement.cc \
       ScorePage.cc \
       ScoreProcess.cc \
       ScoreProcessorNode.cc \
       ScoreRuntime.cc \
       ScoreSegment.cc \
       ScoreSegmentOperatorReadOnly.cc \
       ScoreSegmentOperatorReadWrite.cc \
       ScoreSegmentOperatorSeqReadOnly.cc \
       ScoreSegmentOperatorSeqReadWrite.cc \
       ScoreSegmentOperatorSeqWriteOnly.cc \
       ScoreSegmentOperatorWriteOnly.cc \
       ScoreSegmentReadOnly.cc \
       ScoreSegmentReadWrite.cc \
       ScoreSegmentSeqReadOnly.cc \
       ScoreSegmentSeqReadWrite.cc \
       ScoreSegmentSeqWriteOnly.cc \
       ScoreSegmentStitch.cc \
       ScoreSegmentTable.cc \
       ScoreSegmentWriteOnly.cc \
       ScoreSimulator.cc \
       ScoreStream.cc \
       ScoreStreamStitch.cc \
       ScoreStreamType.cc \
       ScoreSyncEvent.cc \
       ScoreUserSupport.cc \
       ScoreVisualization.cc \
       ScoreGlobalCounter.cc \
       ScorePlayer.cc \
       ScoreFeedbackGraph.cc \
	ScoreAccountingReconfigCmds.cc \
	AllocationTracker.cc \
	ScoreSharedObject_helper.cc \
	ScoreProfiler.cc \
	ScoreOperatorMangle.cc \
	ScoreMangler.cc \
	ScorePlock.cc \
	$(SCHEDULER_SRCS)

RUNTIMESRCS = ScoreArray.cc \
              ScoreArrayPhysicalStatus.cc \
              ScoreCluster.cc \
              ScoreConfig.cc \
              ScoreDummyDonePage.cc \
              ScoreGraphNode.cc \
              ScoreHardwareAPI.cc \
              ScoreOperatorElement.cc \
              ScoreOperatorInstance.cc \
              ScoreOperatorInstanceElement.cc \
              ScorePage.cc \
              ScoreProcess.cc \
              ScoreProcessorNode.cc \
              ScoreRuntime.cc \
              ScoreSegment.cc \
              ScoreSegmentReadOnly.cc \
              ScoreSegmentReadWrite.cc \
              ScoreSegmentSeqReadOnly.cc \
              ScoreSegmentSeqReadWrite.cc \
              ScoreSegmentSeqWriteOnly.cc \
              ScoreSegmentStitch.cc \
              ScoreSegmentTable.cc \
              ScoreSimulator.cc \
              ScoreStream.cc \
              ScoreStreamStitch.cc \
              ScoreStreamType.cc \
              ScoreSyncEvent.cc \
              ScoreSegmentWriteOnly.cc \
              ScoreVisualization.cc \
	      ScoreStateGraph.cc \
	      ScoreFeedbackGraph.cc \
	      ScoreAccountingReconfigCmds.cc \
	      ScoreGlobalCounter.cc \
		AllocationTracker.cc \
		ScoreProfiler.cc \
		ScorePlock.cc \
		$(SCHEDULER_SRCS)

USERSRCS = ScoreCInterface.cc \
           ScoreGraphNode.cc \
           ScoreOperator.cc \
           ScoreOperatorElement.cc \
           ScoreOperatorInstance.cc \
           ScoreOperatorInstanceElement.cc \
           ScorePage.cc \
           ScoreSegment.cc \
           ScoreSegmentOperatorReadOnly.cc \
           ScoreSegmentOperatorReadWrite.cc \
           ScoreSegmentOperatorSeqReadOnly.cc \
           ScoreSegmentOperatorSeqReadWrite.cc \
           ScoreSegmentOperatorSeqWriteOnly.cc \
           ScoreSegmentOperatorWriteOnly.cc \
           ScoreSegmentReadOnly.cc \
           ScoreSegmentReadWrite.cc \
           ScoreSegmentSeqReadOnly.cc \
           ScoreSegmentSeqReadWrite.cc \
           ScoreSegmentSeqWriteOnly.cc \
           ScoreSegmentWriteOnly.cc \
           ScoreStream.cc \
           ScoreStreamType.cc \
           ScoreGlobalCounter.cc  \
	   ScoreUserSupport.cc \
	   ScoreOperatorMangle.cc \
	   AllocationTracker.cc \
	   ScorePlock.cc	

RUNTIMEOBJS = $(addsuffix .o, $(basename $(RUNTIMESRCS)))
USEROBJS = $(addsuffix .o, $(basename $(USERSRCS)))


#the following variable should point to the current location of the xvcg
XVCG_LOCATION=/project/cs/brass/a/tools/free/vcg.1.30/bin/xvcg

.PHONY: export_defines

all: $(OBJS) ScoreRuntime libScoreRuntime.a force_instances ScorePlayer ScoreMangler

force_instances: libScoreRuntime.a
	$(MAKE) TOOL_PATH=$(TOOL_PATH) -f Makefile.instance

ScoreRuntime: $(RUNTIMEOBJS)
	$(CXX) $(GPROFFLAG) -o $@ $^ $(LIB) $(CXXFLAGS)

ScoreRuntime-efence: $(RUNTIMEOBJS)
	$(CXX) -g -o $@ $^ $(LIB) -lefence

ScoreRuntime.a: $(USEROBJS)
	$(AR) $(AR_OPTS) ScoreRuntime.a $^

libScoreRuntime.a: ScoreRuntime.a
	rm -f libScoreRuntime.a
	ln -s ScoreRuntime.a libScoreRuntime.a

ScorePlayer: ScorePlayer.o ScoreStateGraph.o libScoreRuntime.a
	$(CXX) -o $@ $^ -L. -lScoreRuntime $(LIB)

ScoreMangler: ScoreMangler.o ScoreOperatorMangle.o
	$(CXX) -o $@ $^

%.o: %.cc
	$(CXX) -Wno-write-strings -c $(CXXFLAGS) $(INCLUDE) $< -o $@

clean:
	rm -f *.o ScoreRuntime ScoreRuntime-efence *.a ScorePlayer \
		$(addsuffix /*.o, $(SCHEDULER_SRC_TREE))
	$(MAKE) -f Makefile.instance clean

depend: 
	rm -f Makefile.dep
	$(CXX) -MM $(CXXFLAGS) $(INCLUDE) $(SRCS) | ./correct_deps.pl > Makefile.dep
	$(MAKE) -f Makefile.instance depend


-include Makefile.dep

