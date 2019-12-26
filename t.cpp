int a = 3;
int b(){
    return 3;
}
int c(int aaa,int b){
    return b-aaa;
}

int main(){
    int x,y;
    print(b());
    scan(x);
    scan(y);


    print(c(x,y));
}