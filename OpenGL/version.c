#include <stdio.h>
#include <GL/glut.h>

int main(int argc, char** argv)
{
    glutInit(&argc,argv);
    //显示模式初始化
    glutInitDisplayMode(GLUT_SINGLE|GLUT_RGB|GLUT_DEPTH);
    //定义窗口大小
    glutInitWindowSize(300,300);
    //定义窗口位置
    glutInitWindowPosition(100,100);
    //创建窗口
    glutCreateWindow("OpenGL Version");
    glutCreateWindow("OpenGL Version");
    const GLubyte* OpenGLVersion =glGetString(GL_VERSION); //返回当前OpenGL实现的版本号
    printf("OpenGL实现的版本号：%s\n",OpenGLVersion);

    return 0;
}
