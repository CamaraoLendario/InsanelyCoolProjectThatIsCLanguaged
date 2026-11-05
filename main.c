#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "raylib.h"
#include "raymath.h"

#define WINDOW_WIDTH 1200
#define WINDOW_HEIGHT 600
#define TAU (PI*2.)

#define INITIAL_BALL_COUNT 10000
#define BALL_RADIUS 3

#define MAX_QUADTREE_OBJECT_COUNT 2
#define MAX_QUADTREE_RESOLUTION 10

typedef struct BallDef Ball;
typedef struct QuadTreeDef QuadTree;
typedef struct DArrayDef DArray;

struct QuadTreeDef{
    Vector2 RectPos;
    Vector2 RectSize;
    int level;
    bool isRoot;
    QuadTree* parentTree;
    Ball* ballsPtr;
    int ballsCount;
    bool isSubdivided;
    QuadTree* childTrees[4]; //TL, TR, BL, BR
};

struct BallDef{
    Vector2 pos;
    Vector2 velocity;
    float radius;
    int colorIdx;
};

const Color colors[] = {
    {52,  201, 143, 255},
    {231, 76,  60,  255},
    {41,  128, 185, 255},
    {243, 156, 18,  255},
    {155, 89,  182, 255},
    {26,  188, 156, 255},
    {230, 126, 34,  255},
    {52,  73,  94,  255},
    {192, 57,  43,  255},
    {46,  204, 113, 255},
};

const int borders[4] = {
    (-WINDOW_WIDTH*.5) + BALL_RADIUS, // x
    (WINDOW_WIDTH*.5) - BALL_RADIUS,
    (-WINDOW_HEIGHT*.5) + BALL_RADIUS,// y
    (WINDOW_HEIGHT*.5) - BALL_RADIUS
};

Ball balls[INITIAL_BALL_COUNT];
QuadTree* RootTree;
Ball GetNewBall(Vector2 pos, Vector2 velocity, float radius, int colorIdx);
void DrawBalls();
void GenerateBalls();
void ProcessBallPhysics();
void CollideBalls(Ball* b1, Ball* b2);
Vector2 GetVecByAngle(float angle);
Vector2 RandVec();
float Randf();
Texture2D dinoText;

QuadTree** CheckSubdivideQuadTree(QuadTree* quadTree);
QuadTree** SubdivideQuadTree(QuadTree* quadTree);
QuadTree* QuadTreeCreate(Vector2 corner, QuadTree* parentTree, Ball* ballsPtr, int ballsCount);
Vector2 GetQuadTreeCenterPoint(QuadTree* quadTree);
void DrawQuadTree(QuadTree* quadTree);
void FreeQuadtree(QuadTree* quadtree);
void ProcessQuadTreeCollisions(QuadTree* quadtree);
void ProcessBruteForceCollisions();

int main(void) {
    
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "miau ragebait simulator");
    SetTargetFPS(240);
    srand(time(NULL));
    GenerateBalls();
    RootTree = QuadTreeCreate((Vector2){0., 0.}, NULL, balls, INITIAL_BALL_COUNT);
    CheckSubdivideQuadTree(RootTree);
    dinoText = LoadTexture("DinoxaurrStill.png");
    while(!WindowShouldClose()){
        //clock_t startTime = clock();
        ProcessBallPhysics();
        
        BeginDrawing();
        {
            ClearBackground(BLACK);
            DrawBalls();    
            DrawQuadTree(RootTree);
            //clock_t endTime = clock();
            DrawText(TextFormat("%d", GetFPS()), 0., 0., 50, WHITE);
            //DrawText(TextFormat("%d", (((double)(endTime - startTime))/CLOCKS_PER_SEC)*1000.), 0., 55., 50, WHITE);
        }
        EndDrawing();
    }
    
    CloseWindow();
    
    return 0;
}

Ball GetNewBall(Vector2 pos, Vector2 velocity, float radius, int colorIdx){
    return (Ball){pos, velocity, radius, colorIdx};
}
void DrawBalls(){
    for (int i = 0; i < INITIAL_BALL_COUNT; i++){
        Ball currentBall = balls[i];
        DrawTexture(dinoText,
            WINDOW_WIDTH*.5 + currentBall.pos.x,
            WINDOW_HEIGHT*.5 + currentBall.pos.y, WHITE);
        /* DrawPixel(
            WINDOW_WIDTH*.5 + currentBall.pos.x,
            WINDOW_HEIGHT*.5 + currentBall.pos.y,
            colors[currentBall.colorIdx%10]   
        );
        DrawCircle(
            WINDOW_WIDTH*.5 + currentBall.pos.x,
            WINDOW_HEIGHT*.5 + currentBall.pos.y,
            currentBall.radius,
            colors[currentBall.colorIdx%10]
        ); */
    }
}

void GenerateBalls(){
    for (int i = 0; i < INITIAL_BALL_COUNT; i++){
        float ballFraction = (((i/4)+1)/((float)INITIAL_BALL_COUNT/4));
        balls[i] = GetNewBall(
            (Vector2){((Randf()*2.) -1.) * WINDOW_WIDTH*.5, ((Randf()*2.) -1.) * WINDOW_HEIGHT*.5},
            Vector2Scale(RandVec(), 1.75 + Randf()*0.25),
            BALL_RADIUS, i
        );
    }
}

void ProcessBallPhysics(){
    FreeQuadtree(RootTree);
    RootTree = QuadTreeCreate((Vector2){0., 0.}, NULL, balls, INITIAL_BALL_COUNT); // might not be the best practice? but doesn't really matter right now
    CheckSubdivideQuadTree(RootTree);
    for(int i = 0; i < INITIAL_BALL_COUNT; i++){
        // move along velocity
        balls[i].pos = Vector2Add(balls[i].pos, balls[i].velocity);
        // collide with screen borders
        if (balls[i].pos.x <= borders[0] && balls[i].velocity.x < 0 ||
            balls[i].pos.x >= borders[1] && balls[i].velocity.x > 0){
                balls[i].velocity.x *= -1;
        } 
        if (balls[i].pos.y <= borders[2] && balls[i].velocity.y < 0 ||
            balls[i].pos.y >= borders[3] && balls[i].velocity.y > 0 ){
                balls[i].velocity.y *= -1;
        }
    }
    clock_t startTime = clock();
    ProcessQuadTreeCollisions(RootTree);
    // this method costs 41-42ms with spikes of 43ms at 2500 balls and radius 5
    //ProcessBruteForceCollisions();
    clock_t endTime = clock();
    printf("Physics process time: %f\n", (((double)(endTime - startTime))/CLOCKS_PER_SEC)*1000.);
    
}
void ProcessQuadTreeCollisions(QuadTree* quadtree){
    if (!quadtree->isSubdivided){
        for(int i = 0; i < quadtree->ballsCount; i++){
            for(int j = i+1; j < quadtree->ballsCount; j++){
                CollideBalls(&quadtree->ballsPtr[i], &quadtree->ballsPtr[j]);
            }
        }   
    }
    else{
        for(int i = 0; i < 4; i++){
            ProcessQuadTreeCollisions(quadtree->childTrees[i]);
        }
    }
}
void ProcessBruteForceCollisions(){
    for(int i = 0; i < INITIAL_BALL_COUNT; i++){
        for(int j = i+1; j < INITIAL_BALL_COUNT; j++){
                CollideBalls(&balls[i], &balls[j]);
            }
    }   
}

void CollideBalls(Ball* b1, Ball* b2){
    if (Vector2LengthSqr(Vector2Subtract(b1->pos, b2->pos)) > BALL_RADIUS*2 * BALL_RADIUS*2)
        return;
        
    Vector2 b2DirVec = Vector2Normalize(Vector2Subtract(b1->pos, b2->pos));
    Vector2 b1DirVec = Vector2Scale(b2DirVec, -1.);

    if(
        Vector2DotProduct(b1->velocity, b2DirVec) >= 0 &&
        Vector2DotProduct(b2->velocity, b1DirVec) >= 0
    )
    return;

    Vector2 b1Tob2Vel = Vector2Scale(b2DirVec, Vector2DotProduct(b1->velocity, b2DirVec));
    Vector2 b2Tob1Vel = Vector2Scale(b1DirVec, Vector2DotProduct(b2->velocity, b1DirVec));

    

    Vector2 b1NewVel = Vector2Subtract(b1->velocity, b1Tob2Vel);
    b1NewVel = Vector2Add(b1NewVel, b2Tob1Vel);

    Vector2 b2NewVel = Vector2Subtract(b2->velocity, b2Tob1Vel);
    b2NewVel = Vector2Add(b2NewVel, b1Tob2Vel);

    b1->velocity = b1NewVel;
    b2->velocity = b2NewVel;
    
    float ballDist = Vector2Length(Vector2Subtract(b1->pos, b2->pos));
    b1->pos = Vector2Subtract(b1->pos, Vector2Scale(b2DirVec, (ballDist - BALL_RADIUS*2)*.5));
    b2->pos = Vector2Subtract(b2->pos, Vector2Scale(b1DirVec, (ballDist - BALL_RADIUS*2)*.5));
}

Vector2 GetVecByAngle(float angle){
        return (Vector2){
            cos(angle),
            sin(angle)
        };
}
Vector2 RandVec(){
    return GetVecByAngle(Randf() * TAU);
}
float Randf(){
    return rand()/(float)RAND_MAX;
}

QuadTree** CheckSubdivideQuadTree(QuadTree* quadTree){
    if (quadTree->ballsCount <= MAX_QUADTREE_OBJECT_COUNT || quadTree->level >= MAX_QUADTREE_RESOLUTION){
        quadTree->isSubdivided = false;
        return NULL;
    }

    return SubdivideQuadTree(quadTree);
}
QuadTree** SubdivideQuadTree(QuadTree* quadTree){
    int* ballCornerLegend = calloc(quadTree->ballsCount, sizeof(int));
    int cornersBallCount[4] = {0};
    Ball* ballsBuffer =  calloc(quadTree->ballsCount, sizeof(Ball));
    
    for(int i = 0; i < quadTree->ballsCount; i++){
        int corner = 0;
        Vector2 centerPoint = GetQuadTreeCenterPoint(quadTree);
        
        if (quadTree->ballsPtr[i].pos.x > centerPoint.x)
            corner += 2;
        if (quadTree->ballsPtr[i].pos.y > centerPoint.y)
            corner += 1;
        
        ballCornerLegend[i] = corner;
        cornersBallCount[corner]++;
        ballsBuffer[i] = quadTree->ballsPtr[i];
    }
    int nextBallAdr[4] = {
        0,
        cornersBallCount[0],
        cornersBallCount[0] + cornersBallCount[1],
        cornersBallCount[0] + cornersBallCount[1] + cornersBallCount[2],
    };
    for(int i = 0; i < quadTree->ballsCount; i++){
        int corner = ballCornerLegend[i];
        int ballAdrOffset = nextBallAdr[corner]++;
        quadTree->ballsPtr[ballAdrOffset] = ballsBuffer[i];
    } 
    free(ballCornerLegend);
    free(ballsBuffer);

    quadTree->childTrees[0] = QuadTreeCreate((Vector2){0, 0}, quadTree, quadTree->ballsPtr, cornersBallCount[0]);
    quadTree->childTrees[1] = QuadTreeCreate((Vector2){0, 1}, quadTree, quadTree->ballsPtr + nextBallAdr[0], cornersBallCount[1]);
    quadTree->childTrees[2] = QuadTreeCreate((Vector2){1, 0}, quadTree, quadTree->ballsPtr + nextBallAdr[1], cornersBallCount[2]);
    quadTree->childTrees[3] = QuadTreeCreate((Vector2){1, 1}, quadTree, quadTree->ballsPtr + nextBallAdr[2], cornersBallCount[3]);
    for(int i = 0; i < 4; i++)
        CheckSubdivideQuadTree(quadTree->childTrees[i]);
    

    quadTree->isSubdivided = true;
    return quadTree->childTrees;
}

QuadTree* QuadTreeCreate(Vector2 corner, QuadTree* parentTree, Ball* ballsPtr, int ballsCount){ 
    QuadTree* newQuadTreePtr = malloc(sizeof(QuadTree));
    if (parentTree != NULL){
        newQuadTreePtr->RectSize = Vector2Scale(parentTree->RectSize, .5);
        newQuadTreePtr->RectPos = Vector2Add(parentTree->RectPos, Vector2Multiply(newQuadTreePtr->RectSize, corner));
        newQuadTreePtr->level = parentTree->level + 1;
        newQuadTreePtr->isRoot = false;
    }
    else{
        newQuadTreePtr->RectPos = (Vector2){-WINDOW_WIDTH*.5, -WINDOW_HEIGHT*.5};
        newQuadTreePtr->RectSize = (Vector2){WINDOW_WIDTH, WINDOW_HEIGHT};
        newQuadTreePtr->isRoot = true;
        newQuadTreePtr->level = 1;
    }
    
    newQuadTreePtr->parentTree = parentTree;
    newQuadTreePtr->ballsPtr = ballsPtr;
    newQuadTreePtr->ballsCount = ballsCount;
    newQuadTreePtr->isSubdivided = false;

    return newQuadTreePtr;
}
void DrawQuadTree(QuadTree* quadTree){
    if (!quadTree->isSubdivided){
        Vector2 rectOrigin = Vector2Add(quadTree->RectPos, (Vector2){WINDOW_WIDTH*.5, WINDOW_HEIGHT*.5});
        DrawRectangleLines(rectOrigin.x, rectOrigin.y, quadTree->RectSize.x, quadTree->RectSize.y, RED);
        return;
    }
    for(int i = 0; i < 4; i++)
        DrawQuadTree(quadTree->childTrees[i]);
}

Vector2 GetQuadTreeCenterPoint(QuadTree* quadTree){
    return Vector2Add(quadTree->RectPos, Vector2Scale(quadTree->RectSize, .5));
}

void FreeQuadtree(QuadTree* quadtree){
    if (quadtree->isSubdivided){
        for(int i = 0; i < 4; i++){
            FreeQuadtree(quadtree->childTrees[i]);
        }
    }
    free(quadtree);
}
//(how to ragebait miau 101)