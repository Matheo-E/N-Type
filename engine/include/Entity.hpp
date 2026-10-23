#pragma once

class Entity {
    private:
        int E_Id;
    public:
        explicit Entity(int id) : E_Id(id){}
        int Get_Id() const {return E_Id;}
};
