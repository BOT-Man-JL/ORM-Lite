
TARGET = sample
SOURCES = sample.cpp ../src/sqlite3.c
WARNINGFLAGS = -Wall -W
CPPFLAGS = -std=c++14
LINKS = -lstdc++ -lpthread -ldl

OBJS = $(patsubst %.c,%.o,$(patsubst %.cpp,%.o,$(SOURCES)))

%.o: %.c
	$(CC) -c $< -o $@ $(WARNINGFLAGS) $(LINKS)

%.o: %.cpp
	$(CC) -c $< -o $@ $(WARNINGFLAGS) $(LINKS) $(CPPFLAGS)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LINKS)

clean:
	rm -rf $(OBJS) $(TARGET) *.db
