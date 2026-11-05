import datetime
d={"k":0,"m":0,"g":0,"t":0,"p":5}.get(input("room name?>").lower(),0)
t=list(map(int, datetime.datetime.now().strftime('%H %M').split(" ")))
print(f"{t[0]+((t[1]-d)//60)}:{((t[1]-d)%60):02d}")
