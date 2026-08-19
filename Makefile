CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -g -std=c++98

NAME = ircserv
RM = rm -rf

SRC_DIR = src
OBJ_DIR = obj

SRCS =	main.cpp \
		src/Server.cpp \
		src/Client.cpp \
		src/utils.cpp \
		src/Parser.cpp \

OBJS = $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(notdir $(SRCS)))

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

all: $(NAME)

clean:
	$(RM) $(OBJ_DIR)

fclean: clean
	$(RM) $(NAME)

re: fclean all

.PHONY: all clean fclean re