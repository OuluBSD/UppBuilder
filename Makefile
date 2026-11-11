CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall
LDFLAGS ?= -lpthread

SOURCES = UppBuilder.cpp Bridge.cpp UppParser.cpp DepGraph.cpp HeaderScanner.cpp UmkDriver.cpp RuntimeDeps.cpp FlagAuditor.cpp UseModel.cpp PolicyEngine.cpp EscBuildParser.cpp ProfileManager.cpp ProviderPlanner.cpp OverlayManager.cpp EmergeEngine.cpp VdbManager.cpp main.cpp
OBJECTS = $(SOURCES:.cpp=.o)
TARGET = uppbuilder

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) $(LDFLAGS) -o $@

%.o: %.cpp UppBuilder.h Bridge.h UppParser.h DepGraph.h HeaderScanner.h UmkDriver.h RuntimeDeps.h FlagAuditor.h UseModel.h PolicyEngine.h EscBuildParser.h ProfileManager.h ProviderPlanner.h OverlayManager.h EmergeEngine.h VdbManager.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

.PHONY: all clean
