# Entity.msg
# Represents any entity in the simulation world

# Entity types
uint8 TYPE_ROBOT = 0
uint8 TYPE_OBJECT = 1
uint8 TYPE_WALL = 2
uint8 TYPE_ZONE = 3

string id
uint8 type

# Position (all entities)
float32 x
float32 y

# Robot-specific
float32 theta           # Heading angle (radians)
float32 v               # Linear velocity
float32 omega           # Angular velocity
bool is_carrying
string carried_object_id

# Object-specific
string color
float32 pickup_radius
bool is_picked

# Wall-specific
float32 width
float32 height

# Zone-specific
string zone_name
float32 radius
