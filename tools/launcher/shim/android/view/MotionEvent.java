package android.view;
/** Input samples for checking the launcher's coordinate mapping and stick latch. */
public class MotionEvent {
 public static final int ACTION_DOWN=0,ACTION_UP=1,ACTION_MOVE=2,ACTION_CANCEL=3,AXIS_X=0,AXIS_Y=1,AXIS_HAT_X=15,AXIS_HAT_Y=16;
 private final int action; private final float x,y,ax,ay;
 public MotionEvent(int action,float x,float y,float ax,float ay){this.action=action;this.x=x;this.y=y;this.ax=ax;this.ay=ay;}
 public int getActionMasked(){return action;}
 public float getX(){return x;} public float getY(){return y;}
 public float getAxisValue(int axis){return axis==AXIS_X?ax:axis==AXIS_Y?ay:0;}
}
