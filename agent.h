/**
 * Framework for NoGo and similar games (C++ 11)
 * agent.h: Define the behavior of variants of the player
 *
 * Author: Theory of Computer Games
 *         Computer Games and Intelligence (CGI) Lab, NYCU, Taiwan
 *         https://cgilab.nctu.edu.tw/
 */

#pragma once
#include <string>
#include <random>
#include <sstream>
#include <map>
#include <type_traits>
#include <algorithm>
#include <fstream>
#include <cmath>
#include <climits>
#include <float.h>
#include "board.h"
#include "action.h"

class agent {
public:
	agent(const std::string& args = "") {
		std::stringstream ss("name=unknown role=unknown " + args);
		for (std::string pair; ss >> pair; ) {
			std::string key = pair.substr(0, pair.find('='));
			std::string value = pair.substr(pair.find('=') + 1);
			meta[key] = { value };
		}
	}
	virtual ~agent() {}
	virtual void open_episode(const std::string& flag = "") {}
	virtual void close_episode(const std::string& flag = "") {}
	virtual action take_action(const board& b) { return action(); }
	virtual bool check_for_win(const board& b) { return false; }

public:
	virtual std::string property(const std::string& key) const { return meta.at(key); }
	virtual void notify(const std::string& msg) { meta[msg.substr(0, msg.find('='))] = { msg.substr(msg.find('=') + 1) }; }
	virtual std::string name() const { return property("name"); }
	virtual std::string role() const { return property("role"); }

protected:
	typedef std::string key;
	struct value {
		std::string value;
		operator std::string() const { return value; }
		template<typename numeric, typename = typename std::enable_if<std::is_arithmetic<numeric>::value, numeric>::type>
		operator numeric() const { return numeric(std::stod(value)); }
	};
	std::map<key, value> meta;
};

/**
 * base agent for agents with randomness
 */
class random_agent : public agent {
public:
	random_agent(const std::string& args = "") : agent(args) {
		if (meta.find("seed") != meta.end())
			engine.seed(int(meta["seed"]));
	}
	virtual ~random_agent() {}

protected:
	std::default_random_engine engine;
};

/**
 * random player for both side
 * put a legal piece randomly
 */
class player : public random_agent {
public:
	player(const std::string& args = "") : random_agent("name=random role=unknown " + args),
		space(board::size_x * board::size_y), who(board::empty) {
		if (name().find_first_of("[]():; ") != std::string::npos)
			throw std::invalid_argument("invalid name: " + name());
		if (role() == "black") who = board::black;
		if (role() == "white") who = board::white;
		if (who == board::empty)
			throw std::invalid_argument("invalid role: " + role());
		for (size_t i = 0; i < space.size(); i++)
			space[i] = action::place(i, who);
	}

	virtual action take_action(const board& state) {
		std::shuffle(space.begin(), space.end(), engine);
		for (const action::place& move : space) {
			board after = state;
			if (move.apply(after) == board::legal)
				return move;
		}
		return action();
	}

private:
	std::vector<action::place> space;
	board::piece_type who;
};

class MCTSplayer : public random_agent {
public:
	MCTSplayer(const std::string& args = "") : random_agent("name=random role=unknown " + args),
		space(board::size_x * board::size_y), who(board::empty) {
		if (name().find_first_of("[]():; ") != std::string::npos)
			throw std::invalid_argument("invalid name: " + name());
		if (role() == "black") who = board::black;
		if (role() == "white") who = board::white;
		if (who == board::empty)
			throw std::invalid_argument("invalid role: " + role());
		for (size_t i = 0; i < space.size(); i++)
			space[i] = action::place(i, who);
		if(meta.find("mcts") != meta.end())
			std::cout<<"mcts player init"<<std::endl;
		if(meta.find("T") != meta.end())
			simulation_times = meta["T"];
		std::cout<<"simulation_times: "<< simulation_times <<std::endl;
	}


private:
	std::vector<action::place> space;
	board::piece_type who;
	int simulation_times = 1000;

public:
	class Node
		{
			public:
				Node *parent = nullptr;
				std::vector<Node*> children;
				board::piece_type placer = board::black;
				action::place node_move;
				int n = 0, w = 0;
				Node():children(){};
				~Node()
				{
					for(Node* child : children)
						child->~Node();
				};
				bool is_unvisited()
				{
					// std::cout<<n<<std::endl;
					if(n==0)
						return true;
					else 
						return false;
				}
		};

	Node *select_child(board& state, Node *node, Node *root, double c = sqrt(2.0))
	{
		double uct_score;
		double max_score = -1;
		Node *best_child;
		// std::cout<<"select child"<<std::endl;
		if(node->children.size()==0)
		{
			// std::cout<<"select child error"<<std::endl;
			return NULL;
		}
		// std::cout<<"select child for"<<std::endl;
		for(Node* child : node->children)
		{
			if(child->is_unvisited())
				uct_score = DBL_MAX;
			else
				uct_score = (child->w / child->n) + c * sqrt(log(root->n)/child->n);
			if(uct_score > max_score)
			{
				max_score = uct_score;
				best_child = child;
			}
		}
		// best_child->node_move.apply(state);
		state.place(best_child->node_move.position());
		// std::cout<<state<<std::endl;
		// std::cout<<"select child end"<<std::endl;
		return best_child;
	}

	Node *select(board& state, Node *root)
	{
		Node *node = root;
		while(node->children.size() != 0)
		{
			// std::cout<<"select while"<<std::endl;
			// if(node->node_move == select_child(state, node, root)->node_move)
			// 	std::cout<<"error: select same node"<<std::endl;
			node = select_child(state, node, root, 0.5);
			// std::cout<<node->children.size()<<std::endl;;
		}
			
		return node;
	}

	bool expand(const board& state, Node* node)
	{
		std::shuffle(space.begin(), space.end(), engine);
		// std::cout<<"start expand for"<<std::endl;
		for (const action::place& move : space) 
		{	
			Node* newNode = new Node();
			board after = state;
			board::piece_type current_placer = reverse_player(node->placer);
			if(after.place(move.position()) == board::legal)
			{
				newNode->node_move = move;
				newNode->placer = current_placer;
				newNode->parent = node;
				node->children.emplace_back(newNode);
				// if(node->node_move == newNode->node_move)
				// {
				// 	std::cout<<"error: child has same move"<<std::endl;
				// 	std::cout<<state<<std::endl<<after<<std::endl;
				// 	std::cout<<node->node_move.position()<<std::endl;
				// }
 				// if(newNode->n !=0)
				// 	std::cout<<"error: newNode n is not 0";
			}
		}
		// std::cout<<"after expand"<<node->children.size()<<std::endl;
		if(node->children.size()==0)
			return true;
		else
			return false;
	}

	int simulate(const board& state, Node* node)
	{
		board simulate_board(state);
		bool has_leagal_move = true;
		while(has_leagal_move)
		{
			has_leagal_move = false;
			std::shuffle(space.begin(), space.end(), engine);
			for (const action::place& move : space) 
			{	
				board after(simulate_board);
 				if (after.place(move.position()) == board::legal)
				{
					simulate_board.place(move.position());
					// move.apply(simulate_board);
					has_leagal_move = true;
				}
			}
		}
		if(simulate_board.get_who_take_turn() == who)
			return 0;
		else	
			return 1;
	}

	bool backpropagation(Node* node, int result)
	{
		if(node==nullptr)
			return false;
		while(node!=nullptr)
		{
			// if(node->n>5000000 || node->n < 0)
			// 	std::cout<<"error n is not normal"<<std::endl;
			node->n++;
			node->w+=result;
			node = node->parent;
		}
		return true;
	}

	virtual action take_action(const board& state)
	{
		// std::cout<<"--------take acion-------"<<std::endl;
		Node *root = new Node();
		Node *current_node;
		// std::cout<<root->w<<std::endl;
		root->placer = reverse_player(who);
		for(int i=0; i<simulation_times;i++)
		{
			board current_board(state);
			//select
			current_node = select(current_board, root);
			//expand
			if(current_node->n==0)
			{
				// if(expand(current_board, current_node))
				// 	std::cout<<"expand error"<<std::endl;
				expand(current_board, current_node);
				// current_node = current_node->children[0];
				current_node->node_move.apply(current_board);
			}
			//simulate
			int result = simulate(current_board, current_node); 

			//backpropagation
			backpropagation(current_node, result);
		}
		board current_board(state);
		Node *best_node = select_child(current_board, root, root, 0.000000000001);
		// for(Node *child:root->children)
		// 	{

		// 	}
		// std::cout<<state<<std::endl;
		if(best_node)
			return best_node->node_move;
		else
		{
			// std::cout<<state<<std::endl;
			return action();
		}
			
	}
	
	board::piece_type reverse_player(board::piece_type one_side)
	{
		board::piece_type opp_side;
		if(one_side == board::black)
			opp_side = board::white;
		else if(one_side == board::white)
			opp_side = board::black;
		return opp_side;
	}
};

	