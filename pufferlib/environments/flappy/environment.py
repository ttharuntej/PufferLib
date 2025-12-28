# Import the necessary libraries
import gymnasium
import numpy as np
import pufferlib
import functools

def env_creator(name='flappy'):
    return functools.partial(make, name)

def make(name, render_mode='ansi', buf=None, seed=0):
    '''Flappy environment creation function'''
    return Flappy(render_mode=render_mode, buf=buf, seed=seed)

# Define our actions as constants to make the code readable
# For our simple game, we only need UP and DOWN
UP = 0
DOWN = 1

# This is the main class for our environment
# It inherits from pufferlib.PufferEnv
class Flappy(pufferlib.PufferEnv):
# This is the __init__ method we are filling in
    def __init__(self, render_mode='ansi', buf=None, seed=0):
        # First, we define our game's properties
        self.grid_width = 10
        self.grid_height = 2

        # --- 1. What the agent can DO (Action Space) ---
        # The agent has 2 possible actions: UP (0) and DOWN (1).
        # We use spaces.Discrete for a fixed number of simple choices.
        self.single_action_space = gymnasium.spaces.Discrete(2)

        # --- 2. What the agent can SEE (Observation Space) ---
        # To keep it simple, the agent will only observe its own Y position.
        # Is it in the top row (0) or the bottom row (1)?
        # This is a single number, so we use a Box space with a shape of (1,).
        self.single_observation_space = gymnasium.spaces.Box(
            low=0, high=self.grid_height - 1, shape=(1,), dtype=np.uint8)

        # --- PufferLib Boilerplate ---
        # These are required by PufferLib
        self.render_mode = render_mode
        self.num_agents = 1

        # This line MUST be called after you define your spaces
        super().__init__(buf)

        # This will store the agent's current position
        self.agent_pos = 0

# This is the reset method we are filling in
    def reset(self, seed=0):
        # Set the agent's starting Y position to 0 (the top row)
        self.agent_pos = 0

        # The observation is the agent's position.
        # We put it into the self.observations array that PufferLib uses.
        self.observations[0] = [self.agent_pos]

        # PufferLib requires this function to return the initial observations
        # and an empty list for multi-agent compatibility.
        return self.observations, []

 # This is the step method we are filling in
    def step(self, actions):
        # The 'actions' variable contains the move from our agent's brain.
        # For our single agent, this is actions[0].
        action = actions[0]

        # Move the agent's position
        if action == UP:
            self.agent_pos = 0  # Move to the top row
        elif action == DOWN:
            self.agent_pos = 1  # Move to the bottom row

        # Check for game over conditions. In our simple game,
        # the agent loses if it "moves" into the same spot it's already in.
        # This is a simple way to simulate hitting a wall.
        # A real Flappy Bird would have pipes to check against.
        
        # For this tutorial, we'll just end the game after one move.
        # This is the simplest possible "game over" condition.
        self.terminals[0] = True

        # Assign a reward. Since the game ends no matter what,
        # we'll give a neutral reward of 0 for this simple example.
        # In a real game, you'd give -1 for hitting a wall.
        self.rewards[0] = 0

        # Update the observation with the agent's new position
        self.observations[0] = [self.agent_pos]

        # PufferLib requires this function to return these five values
        # in this order. The info dictionary can be used for debugging.
        info = {}
        return self.observations, self.rewards, self.terminals, self.truncations, info


    def render(self):
        # Create a simple 10x2 grid as a list of lists
        # '.' represents an empty space
        grid = [
            ['.' for _ in range(self.grid_width)],
            ['.' for _ in range(self.grid_width)]
        ]

        # Place the agent 'A' in the grid at its current position
        # For simplicity, we'll always draw the agent in the first column
        grid[self.agent_pos][0] = 'A'

        # Build the grid as a string and return it
        output = []
        output.append('+' + '-' * self.grid_width + '+')
        for row in grid:
            output.append('|' + ''.join(row) + '|')
        output.append('+' + '-' * self.grid_width + '+')
        
        return '\n'.join(output)

    def close(self):
        # This is for any cleanup when the environment is closed
        pass 