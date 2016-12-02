
TARGET = Sample
OBJS = Sample.o sqlite3.o
LINKCPP = -std=c++14 -lstdc++

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LINKCPP) -lpthread -ldl

Sample.o: Sample.cpp
	$(CC) -c $< -o $@ $(LINKCPP) -lpthread -ldl

sqlite3.o: src/sqlite3.c
	$(CC) -c $< -o $@ -lpthread -ldl

clean:
	rm -rf $(OBJS) $(TARGET) *.db