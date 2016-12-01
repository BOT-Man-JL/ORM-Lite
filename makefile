
TARGET = Sample
SOURCES=Sample.cpp src/sqlite3.c
CPPFLAGS = -std=c++14
LINKS = -lstdc++ -lpthread -ldl

CC = gcc
OBJS = $(patsubst %.c,%.o,$(patsubst %.cpp,%.o,$(SOURCES)))

%.o: %.c
	$(CC) -c $< -o $@ $(LINKS)

%.o: %.cpp
	$(CC) -c $< -o $@ $(LINKS) $(CPPFLAGS)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LINKS)

clean:
	rm -rf $(OBJS) $(TARGET) *.db