import datetime
de = {"k":5,"m":5,"g":2,"t":2,"p":5}.get(input("room name?>").lower(),0)
t = list(map(int, datetime.datetime.now().strftime('%H %M').split(" ")))
print(f"{t[0]+((t[1]-de)//60)}:{((t[1]-de)%60):02d}")
