# Action.msg
# Policy output action

float32 linear             # Linear velocity command [-1, 1]
float32 angular            # Angular velocity command [-1, 1]
bool pick                  # Pick action trigger
bool place                 # Place action trigger
