#pragma once

class Entity {
    private:
        int EId;
    public:
        explicit Entity(int id) : EId(id){}
        int GetId() const {return EId;}
};
